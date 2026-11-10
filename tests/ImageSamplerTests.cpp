#include "WheelIcons.h"
#include "SkinTextures.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <imgui_impl_dx11.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <array>
#include <cstdlib>
#include <cmath>
using Microsoft::WRL::ComPtr;
void Require(bool ok,const char* message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
void HR(HRESULT result) {Require(SUCCEEDED(result),"D3D11/WIC failure");}
void WriteStripes(const std::filesystem::path& path,bool material=false) {
    ComPtr<IWICImagingFactory> factory;HR(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;HR(factory->CreateStream(&stream));HR(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;HR(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));HR(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> properties;
    HR(encoder->CreateNewFrame(&frame,&properties));HR(frame->Initialize(properties.Get()));HR(frame->SetSize(256,256));
    GUID format=GUID_WICPixelFormat32bppBGRA;HR(frame->SetPixelFormat(&format));Require(format==GUID_WICPixelFormat32bppBGRA,"BGRA format");
    std::vector<BYTE> pixels(256*256*4,255);
    for(int y=0;y<256;++y)for(int x=0;x<256;++x) {
        const int at=(y*256+x)*4;
        if(material)for(int c=0;c<3;++c)pixels[at+c]=(x%2)*255;
        else pixels[at+3]=(x%2)*255;
    }
    HR(frame->WritePixels(256,256*4,static_cast<UINT>(pixels.size()),pixels.data()));HR(frame->Commit());HR(encoder->Commit());
}
void CheckDefault(const ImDrawList*,const ImDrawCmd*) {
    const auto render=static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    ComPtr<ID3D11SamplerState> actual;render->DeviceContext->PSGetSamplers(0,1,&actual);
    Require(actual.Get()==render->SamplerDefault,"Image scope restores font sampler before text/vector draws");
}
void RunDevice(const std::filesystem::path& root) {
    using namespace Wheel;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    HR(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
    Theme theme;theme.id="sampler-test";theme.iconPack=(root/"icons.ini").string();theme.surfaceTexture=(root/"material.png").string();theme.materialStrength=1;
    const std::array themes{theme};
    Require(IconLibrary::Initialize(device.Get(),context.Get(),themes).textures==1,"Load icon on current device");
    Require(SkinTextures::Initialize(device.Get(),context.Get(),themes).textures==1,"Load material on current device");
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={160,80};io.DeltaTime=1.f/60;
    Require(ImGui_ImplDX11_Init(device.Get(),context.Get()),"Initialize backend");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=160;desc.Height=80;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> output,staging;HR(device->CreateTexture2D(&desc,nullptr,&output));
    ComPtr<ID3D11RenderTargetView> target;HR(device->CreateRenderTargetView(output.Get(),nullptr,&target));
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;HR(device->CreateTexture2D(&desc,nullptr,&staging));
    D3D11_SAMPLER_DESC sentinelDesc{};sentinelDesc.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
    sentinelDesc.AddressU=sentinelDesc.AddressV=sentinelDesc.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sentinelDesc.MaxLOD=3;
    ComPtr<ID3D11SamplerState> sentinel;HR(device->CreateSamplerState(&sentinelDesc,&sentinel));
    auto external=sentinel.Get();context->PSSetSamplers(0,1,&external);context->PSSetSamplers(1,1,&external);
    auto render=[&](bool mip,bool material) {
        ImGui_ImplDX11_NewFrame();ImGui::NewFrame();auto draw=ImGui::GetBackgroundDrawList();
        const int first=draw->CmdBuffer.Size-1;
        if(material) {
            const ImageSampler::Scope sampling(draw,SkinTextures::Sampler());
            draw->AddImage(reinterpret_cast<ImTextureID>(SkinTextures::Surface(theme)),{4,4},{41,41});
        }else {Item item;item.icon=IconKind::Sword;DrawIcon(draw,{22.5f,22.5f},item,37.f/40,IM_COL32_WHITE,&theme);}
        int bindCount=0,resetCount=0;
        for(int i=first;i<draw->CmdBuffer.Size;++i) {
            auto& cmd=draw->CmdBuffer[i];if(!cmd.UserCallback)continue;
            if(cmd.UserCallback==ImDrawCallback_ResetRenderState){++resetCount;continue;}
            ++bindCount;auto sampler=static_cast<ID3D11SamplerState*>(cmd.UserCallbackData);
            D3D11_SAMPLER_DESC actual{};sampler->GetDesc(&actual);
            Require(actual.Filter==D3D11_FILTER_MIN_MAG_MIP_LINEAR && actual.MaxLOD==D3D11_FLOAT32_MAX && actual.AddressU==D3D11_TEXTURE_ADDRESS_CLAMP && actual.AddressV==D3D11_TEXTURE_ADDRESS_CLAMP && actual.AddressW==D3D11_TEXTURE_ADDRESS_CLAMP,"Mip sampler descriptor");
            ComPtr<ID3D11Device> owner;sampler->GetDevice(&owner);Require(owner.Get()==device.Get(),"Sampler recreated on current device");
            // Baseline changes only the binding callback; image geometry and SRV stay identical.
            if(!mip){cmd.UserCallback=ImDrawCallback_ResetRenderState;cmd.UserCallbackData=nullptr;}
        }
        Require(bindCount==1 && resetCount==1,"Exactly one scoped sampler pair per image");
        draw->AddCallback(CheckDefault,nullptr);
        draw->AddText({60,8},IM_COL32_WHITE,"Font");Item builtin;builtin.icon=IconKind::Axe;
        DrawIcon(draw,{85,50},builtin,.5f,IM_COL32_WHITE);
        ImGui::Render();const float clear[4]={0,0,0,0};context->ClearRenderTargetView(target.Get(),clear);
        auto raw=target.Get();context->OMSetRenderTargets(1,&raw,nullptr);ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        for(UINT slot=0;slot<2;++slot){ComPtr<ID3D11SamplerState> restored;context->PSGetSamplers(slot,1,&restored);Require(restored.Get()==sentinel.Get(),"External PS sampler slots preserved");}
        context->CopyResource(staging.Get(),output.Get());D3D11_MAPPED_SUBRESOURCE map{};HR(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map));
        std::vector<BYTE> result(160*80*4);
        for(int y=0;y<80;++y)std::copy_n(static_cast<const BYTE*>(map.pData)+y*map.RowPitch,160*4,result.data()+y*160*4);
        context->Unmap(staging.Get(),0);return result;
    };
    for(bool material:{false,true}) {
        const auto before=render(false,material),after=render(true,material);
        double oldError=0,newError=0;int oldMid=0,newMid=0;
        const int channel=material?0:3; // Material opacity is deliberately always 255.
        for(int y=5;y<40;++y)for(int x=5;x<40;++x) {
            const auto at=(y*160+x)*4+channel;const int a=before[at],b=after[at];
            oldError+=std::abs(a-127.5);newError+=std::abs(b-127.5);oldMid+=a>16 && a<239;newMid+=b>16 && b<239;
        }
        {Require(newError<oldError*.1 && newMid>oldMid,"Mip minification reduces aliasing and increases intermediate coverage");std::cout<<(material?"37px material RGB: mean error ":"37px icon alpha: mean error ")<<oldError/1225<<" -> "<<newError/1225<<", transitions "<<oldMid<<" -> "<<newMid<<'\n';}
        for(int y=0;y<80;++y)for(int x=50;x<160;++x)for(int c=0;c<4;++c)Require(before[(y*160+x)*4+c]==after[(y*160+x)*4+c],"Font and built-in vectors unchanged");
    }
    // Font-atlas rebuilds on the same device must not invalidate the library sampler.
    ImGui_ImplDX11_InvalidateDeviceObjects();Require(ImGui_ImplDX11_CreateDeviceObjects(),"Rebuild font atlas");render(true,false);
    IconLibrary::Reset();SkinTextures::Reset();
    Item item;item.icon=IconKind::Sword;Require(!IconLibrary::Find(theme,item).view && !SkinTextures::Surface(theme),"Reset drops image resources");
    ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();
}
int main() {
    HR(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    const auto root=std::filesystem::path("build")/("image-sampler-test-"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(root);
    WriteStripes(root/"stripes.png");WriteStripes(root/"material.png",true);{std::ofstream manifest(root/"icons.ini");manifest<<"[Icons]\nsword=stripes.png|tint\n";}
    RunDevice(root);RunDevice(root);
    std::filesystem::remove_all(root);CoUninitialize();std::cout<<"ImageSamplerTests passed (two WARP devices)\n";
}


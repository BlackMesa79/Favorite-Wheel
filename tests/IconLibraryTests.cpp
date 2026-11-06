#include "IconLibrary.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <array>
using Microsoft::WRL::ComPtr;
void Check(bool condition,const char* message){if(!condition){std::cerr<<message<<'\n';std::exit(1);}}
void HR(HRESULT result){Check(SUCCEEDED(result),"D3D11/WIC fixture failure");}
void PNG(IWICImagingFactory* factory,const std::filesystem::path& path,UINT width) {
    ComPtr<IWICStream> stream;HR(factory->CreateStream(&stream));
    HR(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;HR(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
    HR(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;
    HR(encoder->CreateNewFrame(&frame,&options));HR(frame->Initialize(options.Get()));HR(frame->SetSize(width,1));
    GUID format=GUID_WICPixelFormat32bppBGRA;HR(frame->SetPixelFormat(&format));Check(format==GUID_WICPixelFormat32bppBGRA,"RGBA fixture format");
    std::vector<BYTE> pixels(width*4,0);
    pixels[0]=0;pixels[1]=0;pixels[2]=0;pixels[3]=0; // Transparent texel becomes white, opaque multiplier.
    if(width>1){pixels[4]=40;pixels[5]=80;pixels[6]=120;pixels[7]=255;}
    HR(frame->WritePixels(1,width*4,static_cast<UINT>(pixels.size()),pixels.data()));HR(frame->Commit());HR(encoder->Commit());
}
int main() {
    using namespace Wheel;
    HR(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    ComPtr<IWICImagingFactory> factory;HR(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    const auto root=std::filesystem::path("build")/("icon-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    PNG(factory.Get(),root/"alpha.png",2);
    {std::ofstream file(root/"icons.ini");file<<"[Icons]\nsword=alpha.png|original\nrestoration=missing.png\nmagic=alpha.png|tint\nspecial=alpha.png|original\nevil=../outside.png\n[Forms]\nSkyrim.esm|00012EB7=special\n";}
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    HR(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    Theme theme;theme.iconPack=(root/"icons.ini").string();std::array themes{theme,theme};
    auto report=IconLibrary::Initialize(device.Get(),context.Get(),themes);
    Check(report.textures==1 && !report.warnings.empty(),"Shared textures, invalid entries diagnosed");
    Item item;item.icon=IconKind::Sword;auto image=IconLibrary::Find(theme,item);
    Check(image.view && !image.tint && image.aspect==2,"Original color and aspect ratio");
    ComPtr<ID3D11Resource> resource;image.view->GetResource(&resource);ComPtr<ID3D11Texture2D> source;HR(resource.As(&source));
    D3D11_TEXTURE2D_DESC desc;source->GetDesc(&desc);desc.MipLevels=1;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;HR(device->CreateTexture2D(&desc,nullptr,&staging));context->CopySubresourceRegion(staging.Get(),0,0,0,0,source.Get(),0,nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped{};HR(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));const auto pixels=static_cast<const BYTE*>(mapped.pData);
    Check(pixels[3]==0 && pixels[4]==120 && pixels[5]==80 && pixels[6]==40 && pixels[7]==255,"Icon upload preserves alpha and color");context->Unmap(staging.Get(),0);
    item.icon=IconKind::Restoration;Check(IconLibrary::Find(theme,item).tint,"Invalid school image falls back to generic magic");
    item.iconForm=IconLibrary::FormKey("SKYRIM.ESM",0x12EB7);Check(!IconLibrary::Find(theme,item).tint,"Stable form override wins over school");
    item.action=ActionKind::SaveOutfit;Check(!IconLibrary::Find(theme,item).view,"Form override cannot replace function icons");
    IconLibrary::Reset();Check(!IconLibrary::Find(theme,item).view,"Reset releases library");
    std::filesystem::remove_all(root);factory.Reset();CoUninitialize();std::cout<<"IconLibraryTests passed\n";
}

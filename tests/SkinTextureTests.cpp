#include "SkinTextures.h"
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
    const auto root=std::filesystem::path("build")/("skin-texture-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    PNG(factory.Get(),root/"alpha.png",2);PNG(factory.Get(),root/"oversized.png",1025);
    {std::ofstream out(root/"bad.png");out<<"not an image";}
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    HR(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    std::array<Theme,6> themes;
    for(int i=0;i<6;++i){themes[i].id=std::to_string(i);themes[i].materialStrength=1;}
    themes[0].surfaceTexture=(root/"alpha.png").string();themes[1].surfaceTexture=themes[0].surfaceTexture;
    themes[2].surfaceTexture=(root/"missing.png").string();themes[3].surfaceTexture=(root/"bad.png").string();
    themes[4].surfaceTexture=(root/"oversized.png").string();themes[5].surfaceTexture=themes[0].surfaceTexture;themes[5].materialStrength=0;
    const auto report=SkinTextures::Initialize(device.Get(),context.Get(),themes);
    Check(report.textures==1 && report.texels==2 && report.warnings.size()==3,"Decode failures/oversize use fallback; shared materials upload once");
    const auto view=SkinTextures::Surface(themes[0]);Check(view && view==SkinTextures::Surface(themes[1]),"Two themes share their GPU resource");
    for(int i=2;i<6;++i)Check(!SkinTextures::Surface(themes[i]),"Invalid/disabled resources have no GPU surface");
    auto changed=themes[0];changed.surfaceTexture="different.png";Check(!SkinTextures::Surface(changed),"Stale resource identity cannot render a previous material");
    ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;HR(resource.As(&texture));
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    Check(desc.MipLevels==2 && desc.Width==2 && desc.Height==1,"Material uploads include the mip chain");
    desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.BindFlags=0;desc.MiscFlags=0;
    ComPtr<ID3D11Texture2D> readback;HR(device->CreateTexture2D(&desc,nullptr,&readback));context->CopyResource(readback.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};HR(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped));
    const auto pixels=static_cast<const BYTE*>(mapped.pData);
    Check(pixels[0]==255 && pixels[1]==255 && pixels[2]==255 && pixels[3]==255 &&
        pixels[4]==120 && pixels[5]==80 && pixels[6]==40 && pixels[7]==255,"PNG alpha changes material intensity, never menu opacity; RGBA channel order is correct");
    context->Unmap(readback.Get(),0);
    std::vector<Theme> budget(20,themes[0]);
    for(int i=0;i<20;++i){budget[i].id=std::to_string(i);budget[i].materialStrength=.05f*(i+1);}
    const auto limited=SkinTextures::Initialize(device.Get(),context.Get(),budget);
    Check(limited.textures==16 && limited.warnings.size()==4,"Unique material uploads respect the texture-count budget");
    SkinTextures::Reset();Check(!SkinTextures::Surface(themes[0]),"Reset drops the cache on the owning renderer thread");
    factory.Reset();CoUninitialize();
    std::cout<<"PNG fallback, alpha, mipmaps, deduplication, budgets and cache lifetime passed\n";
}

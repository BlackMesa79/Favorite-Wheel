#include "SkinTextures.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <unordered_map>

namespace Wheel::SkinTextures {
    namespace {
        using Microsoft::WRL::ComPtr;
        struct Entry {std::string path;float strength=0;ComPtr<ID3D11ShaderResourceView> view;};
        std::unordered_map<std::string,Entry> surfaces;
        constexpr std::size_t maxTextures=16,maxTexels=4*1024*1024;
        constexpr UINT maxDimension=1024;
        ComPtr<ID3D11ShaderResourceView> Load(ID3D11Device* device,ID3D11DeviceContext* context,
            IWICImagingFactory* factory,const Theme& theme,std::size_t& texels) {
            const auto path=std::filesystem::u8path(theme.surfaceTexture);
            std::error_code error;
            const auto size=std::filesystem::file_size(path,error);
            if(error || size>8*1024*1024)return {};
            ComPtr<IWICBitmapDecoder> decoder;
            if(FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder)))return {};
            GUID container{};
            if(FAILED(decoder->GetContainerFormat(&container)) || container!=GUID_ContainerFormatPng)return {};
            ComPtr<IWICBitmapFrameDecode> frame;
            UINT width=0,height=0;
            if(FAILED(decoder->GetFrame(0,&frame)) || FAILED(frame->GetSize(&width,&height)) ||
                !width || !height || width>maxDimension || height>maxDimension ||
                texels+std::size_t(width)*height>maxTexels)return {};
            ComPtr<IWICFormatConverter> converter;
            if(FAILED(factory->CreateFormatConverter(&converter)) ||
                FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return {};
            std::vector<unsigned char> pixels(std::size_t(width)*height*4);
            if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return {};
            const float strength=std::isfinite(theme.materialStrength)?std::clamp(theme.materialStrength,0.f,1.f):0.f;
            for(std::size_t at=0;at<pixels.size();at+=4) {
                // PNG alpha controls material strength, never surface transparency.
                const float blend=strength*pixels[at+3]/255.f;
                for(int channel=0;channel<3;++channel)
                    pixels[at+channel]=static_cast<unsigned char>(std::lround(std::lerp(255.f,float(pixels[at+channel]),blend)));
                pixels[at+3]=255;
            }
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width=width;desc.Height=height;desc.MipLevels=0;desc.ArraySize=1;
            desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
            desc.Usage=D3D11_USAGE_DEFAULT;
            desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
            ComPtr<ID3D11Texture2D> texture;
            ComPtr<ID3D11ShaderResourceView> view;
            if(FAILED(device->CreateTexture2D(&desc,nullptr,&texture)) ||
                FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&view)))return {};
            context->UpdateSubresource(texture.Get(),0,nullptr,pixels.data(),width*4,0);
            context->GenerateMips(view.Get());
            texels+=std::size_t(width)*height;
            return view;
        }
    }
    void Reset() {surfaces.clear();}
    Report Initialize(ID3D11Device* device,ID3D11DeviceContext* context,std::span<const Theme> themes) {
        Reset();Report report;
        if(!device || !context)return report;
        const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        struct COMBalance {bool owned;~COMBalance(){if(owned)CoUninitialize();}} balance{SUCCEEDED(initialized)};
        ComPtr<IWICImagingFactory> factory;
        if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) {
            report.warnings.push_back("PNG decoder unavailable; using solid theme surfaces");return report;
        }
        for(const auto& theme:themes) {
            if(theme.surfaceTexture.empty() || !(theme.materialStrength>0))continue;
            Entry entry{theme.surfaceTexture,theme.materialStrength,{}};
            for(const auto& [id,cached]:surfaces)
                if(cached.path==entry.path && cached.strength==entry.strength) {entry.view=cached.view;break;}
            if(!entry.view && report.textures<maxTextures) {
                entry.view=Load(device,context,factory.Get(),theme,report.texels);
                if(entry.view)++report.textures;
            }
            if(!entry.view)report.warnings.push_back("Theme '"+theme.id+"': missing/invalid PNG or texture budget exceeded; using solid surfaces");
            surfaces.emplace(theme.id,std::move(entry));
        }
        return report;
    }
    ID3D11ShaderResourceView* Surface(const Theme& theme) {
        const auto found=surfaces.find(theme.id);
        if(found==surfaces.end() || found->second.path!=theme.surfaceTexture ||
            found->second.strength!=theme.materialStrength)return nullptr;
        return found->second.view.Get();
    }
}

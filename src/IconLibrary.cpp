#include "IconLibrary.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <charconv>
#include <cstdio>
#include <algorithm>

namespace Wheel::IconLibrary {
namespace {
    using Microsoft::WRL::ComPtr;
    struct Texture {ComPtr<ID3D11ShaderResourceView> view;float aspect=1;};
    struct Entry {std::string path;bool tint=true;};
    struct Pack {std::unordered_map<std::string,Entry> icons;std::unordered_map<std::string,std::string> forms;};
    std::unordered_map<std::string,Pack> packs;
    std::unordered_map<std::string,Texture> textures;
    std::string Trim(std::string s) {const auto a=s.find_first_not_of(" \t\r\n");return a==s.npos?"":s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
    std::string Lower(std::string s) {for(auto& c:s)if(c>='A' && c<='Z')c+=32;return s;}
    std::string UTF8(const std::filesystem::path& p){const auto s=p.generic_u8string();return {s.begin(),s.end()};}
    bool Relative(const std::filesystem::path& p) {
        const auto s=UTF8(p);
        return !p.empty() && !p.has_root_path() && s.find(':')==s.npos &&
            std::none_of(p.begin(),p.end(),[](const auto& part){return part=="..";});
    }
    Texture Load(ID3D11Device* device,ID3D11DeviceContext* context,IWICImagingFactory* factory,const std::string& name,Report& report) {
        Texture result;std::error_code ec;const auto path=std::filesystem::u8path(name);
        if(std::filesystem::file_size(path,ec)>8*1024*1024 || ec || report.textures>=128)return result;
        ComPtr<IWICBitmapDecoder> decoder;ComPtr<IWICBitmapFrameDecode> frame;ComPtr<IWICFormatConverter> converter;
        GUID format{};UINT width=0,height=0;
        if(FAILED(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder)) ||
            FAILED(decoder->GetContainerFormat(&format)) || format!=GUID_ContainerFormatPng || FAILED(decoder->GetFrame(0,&frame)) ||
            FAILED(frame->GetSize(&width,&height)) || !width || !height || width>1024 || height>1024 || report.texels+std::size_t(width)*height>16*1024*1024 ||
            FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))return result;
        std::vector<unsigned char> pixels(std::size_t(width)*height*4);
        if(FAILED(converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data())))return result;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=0;desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
        ComPtr<ID3D11Texture2D> texture;
        if(FAILED(device->CreateTexture2D(&desc,nullptr,&texture)) || FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&result.view)))return {};
        context->UpdateSubresource(texture.Get(),0,nullptr,pixels.data(),width*4,0);context->GenerateMips(result.view.Get());
        result.aspect=float(width)/height;++report.textures;report.texels+=std::size_t(width)*height;return result;
    }
}
std::string FormKey(std::string plugin,std::uint32_t id) {
    if(plugin.empty() || !id)return {};
    char hex[9];std::snprintf(hex,sizeof(hex),"%08X",id);return Lower(std::move(plugin))+"|"+hex;
}
IconKind EffectiveKind(const Item& item) {
    if(item.icon!=IconKind::Auto)return item.icon;
    switch(item.category) {
    case Category::Weapons:return IconKind::Sword;
    case Category::Armor:return IconKind::Armor;
    case Category::Potions:return IconKind::Potion;
    case Category::Poisons:return IconKind::Poison;
    case Category::Food:return IconKind::Food;
    case Category::Spells:return IconKind::Magic;
    case Category::Shouts:return IconKind::Shout;
    case Category::Powers:return IconKind::Power;
    case Category::Books:return IconKind::Book;
    default:return IconKind::Other;
    }
}
const char* Key(const Item& item) {
    switch(item.action) {
    case ActionKind::SaveOutfit:return "save_outfit";
    case ActionKind::ImportOutfits:return "import_outfits";
    case ActionKind::Outfit:return "outfit";
    case ActionKind::Cycle:return "cycle";
    case ActionKind::Combat:case ActionKind::CycleMember:return "combat";
    case ActionKind::SaveCombat:case ActionKind::CycleCapture:return "save_combat";
    default:break;
    }
    switch(EffectiveKind(item)) {
#define ICON(value,key) case IconKind::value:return key;
    ICON(Sword,"sword") ICON(Dagger,"dagger") ICON(Axe,"axe") ICON(Mace,"mace") ICON(Bow,"bow") ICON(Crossbow,"crossbow")
    ICON(Staff,"staff") ICON(Arrow,"arrow") ICON(Armor,"armor") ICON(Robe,"robe") ICON(Helmet,"helmet") ICON(Gloves,"gloves")
    ICON(Boots,"boots") ICON(Ring,"ring") ICON(Amulet,"amulet") ICON(Shield,"shield") ICON(Potion,"potion") ICON(Poison,"poison")
    ICON(Food,"food") ICON(Magic,"magic") ICON(Scroll,"scroll") ICON(Torch,"torch") ICON(LightPlayer,"light_player")
    ICON(LightTarget,"light_target") ICON(LightGroup,"light_group") ICON(Back,"back") ICON(Alteration,"alteration")
    ICON(Conjuration,"conjuration") ICON(Destruction,"destruction") ICON(Illusion,"illusion") ICON(Restoration,"restoration")
    ICON(Shout,"shout") ICON(Power,"power") ICON(Book,"book")
#undef ICON
    default:return "other";
    }
}
void Reset(){packs.clear();textures.clear();}
Report Initialize(ID3D11Device* device,ID3D11DeviceContext* context,std::span<const Theme> themes) {
    Reset();Report report;if(!device || !context)return report;
    const auto initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Balance {bool owned;~Balance(){if(owned)CoUninitialize();}} balance{SUCCEEDED(initialized)};
    ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) {report.warnings.push_back("Icon PNG decoder unavailable");return report;}
    for(const auto& theme:themes) {
        if(theme.iconPack.empty() || packs.contains(theme.iconPack))continue;
        if(packs.size()>=32){report.warnings.push_back("Icon pack limit reached");break;}
        auto& pack=packs[theme.iconPack];const auto manifest=std::filesystem::u8path(theme.iconPack);std::error_code ec;
        if(std::filesystem::file_size(manifest,ec)>256*1024 || ec){report.warnings.push_back("Missing/oversized icon manifest: "+theme.iconPack);continue;}
        std::ifstream input(manifest);std::string line,section;std::size_t entries=0;bool invalid=false;
        while(std::getline(input,line)) {
            if(line.starts_with("\xEF\xBB\xBF"))line.erase(0,3);line=Trim(line);
            if(line.empty() || line[0]==';' || line[0]=='#')continue;
            if(line.front()=='[' && line.back()==']'){section=Lower(line.substr(1,line.size()-2));continue;}
            const auto equal=line.find('=');if(equal==line.npos){invalid=true;continue;}
            if(++entries>512){invalid=true;break;}
            auto key=Trim(line.substr(0,equal)),value=Trim(line.substr(equal+1));
            if(section=="icons") {
                bool tint=true;const auto bar=value.rfind('|');
                if(bar!=value.npos){const auto mode=Lower(Trim(value.substr(bar+1)));if(mode!="tint" && mode!="original"){invalid=true;continue;}tint=mode=="tint";value=Trim(value.substr(0,bar));}
                const auto relative=std::filesystem::u8path(value);
                if(key.empty() || !Relative(relative) || Lower(UTF8(relative.extension()))!=".png"){invalid=true;continue;}
                pack.icons[Lower(key)]={UTF8((manifest.parent_path()/relative).lexically_normal()),tint};
            }else if(section=="forms") {
                const auto bar=key.rfind('|');if(bar==key.npos || bar==0){invalid=true;continue;}
                auto hex=Trim(key.substr(bar+1));if(hex.starts_with("0x") || hex.starts_with("0X"))hex.erase(0,2);
                std::uint32_t id=0;const auto parsed=std::from_chars(hex.data(),hex.data()+hex.size(),id,16);
                if(parsed.ec!=std::errc{} || parsed.ptr!=hex.data()+hex.size() || !id || id>0xFFFFFF){invalid=true;continue;}
                pack.forms[FormKey(Trim(key.substr(0,bar)),id)]=Lower(value);
            }else invalid=true;
        }
        // Sorted loading makes resource-budget fallback deterministic.
        std::vector<std::string> paths;for(const auto& [key,entry]:pack.icons)paths.push_back(entry.path);
        std::sort(paths.begin(),paths.end());paths.erase(std::unique(paths.begin(),paths.end()),paths.end());
        for(const auto& path:paths)if(!textures.contains(path)) {
            auto texture=Load(device,context,factory.Get(),path,report);
            if(!texture.view)invalid=true;textures.emplace(path,std::move(texture));
        }
        if(invalid)report.warnings.push_back("Icon pack has invalid/missing entries or exceeded resource limits; using fallback: "+theme.iconPack);
    }
    return report;
}
Image Find(const Theme& theme,const Item& item) {
    const auto found=packs.find(theme.iconPack);if(found==packs.end())return {};
    const auto& pack=found->second;
    auto get=[&](const std::string& key)->Image {
        const auto entry=pack.icons.find(key);if(entry==pack.icons.end())return {};
        const auto texture=textures.find(entry->second.path);if(texture==textures.end() || !texture->second.view)return {};
        return {texture->second.view.Get(),texture->second.aspect,entry->second.tint};
    };
    if(item.action==ActionKind::Favorite)if(const auto mapping=pack.forms.find(item.iconForm);mapping!=pack.forms.end())
        if(auto image=get(mapping->second);image.view)return image;
    if(auto image=get(Key(item));image.view)return image;
    const auto kind=EffectiveKind(item);
    if(item.action==ActionKind::Favorite && kind>=IconKind::Alteration && kind<=IconKind::Power)return get("magic");
    return {};
}
}

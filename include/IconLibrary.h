#pragma once
#include "Favorites.h"
#include "ImageSampler.h"
#include "UIResources.h"
#include <span>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace Wheel::IconLibrary {
    struct Image {ID3D11ShaderResourceView* view=nullptr; float aspect=1; bool tint=true; const ImageSampler* sampler=nullptr; std::optional<std::uint32_t> color;};
    inline std::uint32_t ImageColor(const Image& image,std::uint32_t ink) {
        const auto alpha=ink&0xFF000000u;
        if(image.color) {const auto rgb=*image.color;return alpha|((rgb&255)<<16)|(rgb&0xFF00)|((rgb>>16)&255);}
        return image.tint?ink:alpha|0x00FFFFFFu;
    }
    struct Report {std::size_t textures=0,texels=0;std::vector<std::string> warnings;};
    // Renderer-thread ownership; no disk or game access in Find.
    Report Initialize(ID3D11Device*,ID3D11DeviceContext*,std::span<const Theme>);
    void Reset();
    Image Find(const Theme&,const Item&);
    std::string FormKey(std::string plugin,std::uint32_t localID);
    IconKind EffectiveKind(const Item&);
    const char* Key(const Item&);
}

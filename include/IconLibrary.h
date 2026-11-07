#pragma once
#include "Favorites.h"
#include "UIResources.h"
#include <span>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace Wheel::IconLibrary {
    struct Image {ID3D11ShaderResourceView* view=nullptr; float aspect=1; bool tint=true;};
    struct Report {std::size_t textures=0,texels=0;std::vector<std::string> warnings;};
    // Renderer-thread ownership; no disk or game access in Find.
    Report Initialize(ID3D11Device*,ID3D11DeviceContext*,std::span<const Theme>);
    void Reset();
    Image Find(const Theme&,const Item&);
    std::string FormKey(std::string plugin,std::uint32_t localID);
    IconKind EffectiveKind(const Item&);
    const char* Key(const Item&);
}

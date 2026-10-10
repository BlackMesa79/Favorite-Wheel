#pragma once
#include "UIResources.h"
#include <cstddef>
#include <span>
#include <string>
#include <vector>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace Wheel::SkinTextures {
    struct Report {
        std::size_t textures=0,texels=0;
        std::vector<std::string> warnings;
    };
    // Renderer-thread-only. Decode/upload once at initialization, never during drawing.
    Report Initialize(ID3D11Device*,ID3D11DeviceContext*,std::span<const Theme>);
    ID3D11ShaderResourceView* Surface(const Theme&);
    void Reset();
}

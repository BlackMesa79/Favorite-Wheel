#include "ImageSampler.h"
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>

namespace Wheel {
namespace {
    void BindImageSampler(const ImDrawList*, const ImDrawCmd* command) {
        const auto render = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
        if (!render) return;
        auto sampler = static_cast<ID3D11SamplerState*>(command->UserCallbackData);
        // Only PS slot 0 changes; the backend owns the context and all other state.
        render->DeviceContext->PSSetSamplers(0, 1, &sampler);
    }
}
ImageSampler::~ImageSampler() { Reset(); }
void ImageSampler::Reset() {
    if (state) { state->Release(); state = nullptr; }
}
bool ImageSampler::Initialize(ID3D11Device* device) {
    Reset();
    if (!device) return false;
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = desc.AddressV = desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MaxAnisotropy = 1;
    desc.MaxLOD = D3D11_FLOAT32_MAX;
    return SUCCEEDED(device->CreateSamplerState(&desc, &state));
}
ImageSampler::Scope::Scope(ImDrawList* list, const ImageSampler* sampler) {
    if (sampler && sampler->state) {
        draw = list;
        draw->AddCallback(BindImageSampler, sampler->state);
    }
}
ImageSampler::Scope::~Scope() {
    // End before subsequent font-atlas/vector draws, including early returns.
    if (draw) draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}
}

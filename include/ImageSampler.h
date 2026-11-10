#pragma once
struct ID3D11Device;
struct ID3D11SamplerState;
struct ImDrawList;

namespace Wheel {
// Owned by the texture library, with the same lifetime as its queued image views.
class ImageSampler {
public:
    ImageSampler() = default;
    ~ImageSampler();
    ImageSampler(const ImageSampler&) = delete;
    ImageSampler& operator=(const ImageSampler&) = delete;
    bool Initialize(ID3D11Device* device);
    void Reset();
    class Scope {
    public:
        Scope(ImDrawList* draw, const ImageSampler* sampler);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        ImDrawList* draw = nullptr;
    };
private:
    ID3D11SamplerState* state = nullptr;
};
}

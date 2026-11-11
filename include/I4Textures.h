#pragma once
#include "I4Runtime.h"
#include "ImageSampler.h"
#include <functional>
#include <optional>
#include <span>
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace Wheel::I4Textures {
    using Reader = std::function<std::optional<std::vector<std::uint8_t>>(const std::string&)>;
    using Scheduler = std::function<void(std::function<void()>)>;
    using Logger = std::function<void(const std::string&)>;
    // Startup only. Scheduler dispatches to the game thread; Reader is never called elsewhere.
    void Configure(Reader, Scheduler, Logger);
    // Game thread: only copy identities and schedule work, never read resources here.
    void RequestSnapshot(std::span<const I4::Icon>);
    // Render thread. Reset invalidates GPU work; parsed movies and failures survive device loss.
    void Initialize(ID3D11Device*, ID3D11DeviceContext*);
    void Reset();
    void BeginFrame(float maximumIconPixels);
    struct Image { ID3D11ShaderResourceView* view=nullptr; const ImageSampler* sampler=nullptr; };
    Image Find(const I4::Icon&);
    struct Statistics { std::size_t gpuBytes=0, textures=0, pending=0, ready=0; unsigned uploaded=0; std::size_t parses=0, renders=0; };
    Statistics Stats();
    void Shutdown(); // Offline tests: stop/join outside loader lock; shared state is never destroyed.
}

#include "Wheel.h"
#include "Settings.h"
#include "WheelFonts.h"
#include "Transition.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <chrono>
#include <filesystem>

namespace Wheel {
    namespace {
        using Microsoft::WRL::ComPtr;
        using Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
        Present previousPresent = nullptr;
        IDXGISwapChain* targetSwapChain = nullptr;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        ImGuiContext* imgui = nullptr;
        std::atomic<bool> ready{false};
        bool failed = false;
        bool fading = false;
        auto lastFrame = std::chrono::steady_clock::now();

        bool Initialize(IDXGISwapChain* chain) {
            if (FAILED(chain->GetDevice(IID_PPV_ARGS(device.GetAddressOf())))) return false;
            device->GetImmediateContext(context.GetAddressOf());
            imgui = ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            // This plugin consumes engine input directly and has no Win32 backend or WndProc hook.
            const auto& font = Config().font;
            io.Fonts->AddFontDefault();
            if (!std::filesystem::exists(font)) {
                SKSE::log::warn("CJK font unavailable: {}. Configure Display/Font if Chinese text is missing.", font);
            }
            if (!ImGui_ImplDX11_Init(device.Get(), context.Get()) || !ImGui_ImplDX11_CreateDeviceObjects()) return false;
            ready = true;
            SKSE::log::info("Independent D3D11 renderer ready");
            return true;
        }

        void Render(IDXGISwapChain* chain) {
            static View lastView;
            static Transition transition;
            static bool wasOpen=false;
            static auto tick=std::chrono::steady_clock::now();
            const auto nowTick=std::chrono::steady_clock::now();
            float elapsed=std::clamp(std::chrono::duration<float>(nowTick-tick).count(),0.f,.25f);
            tick=nowTick;
            AdvanceGamepadPointer(elapsed);
            const auto current=Snapshot();
            if(current.open && !wasOpen) {elapsed=0;ResetVisualFeedback();}
            wasOpen=current.open;
            const float opacity=transition.Update(current.open,current.config.animations,current.animateClose,elapsed);
            fading=opacity>0 || current.open;
            if(current.open) lastView=current;
            if(opacity<=0 || !lastView.open) return;
            const auto& view=lastView;
            // Follow the engine UI framebuffer. CS redirects this RTV to its separate UI
            // layer; swapChain->GetBuffer() instead points to the scene beneath that layer.
            ComPtr<ID3D11RenderTargetView> target;
            if(auto renderer=RE::BSGraphics::Renderer::GetSingleton())
                // CommonLib's REX declaration shares the native COM ABI. Assignment retains the engine-owned view.
                target=reinterpret_cast<ID3D11RenderTargetView*>(
                    renderer->GetRuntimeData().renderTargets[RE::RENDER_TARGET::kFRAMEBUFFER].RTV);
            ComPtr<ID3D11Texture2D> buffer;
            const bool engineTarget=bool(target);
            if(target) {
                ComPtr<ID3D11Resource> resource;
                target->GetResource(resource.GetAddressOf());
                if(FAILED(resource.As(&buffer))) return;
            } else {
                if(FAILED(chain->GetBuffer(0,IID_PPV_ARGS(buffer.GetAddressOf())))) return;
                if(FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,target.GetAddressOf()))) return;
            }
            D3D11_TEXTURE2D_DESC desc{};
            buffer->GetDesc(&desc);
            if(!desc.Width || !desc.Height) return;
            static UINT loggedWidth=0,loggedHeight=0;
            static DXGI_FORMAT loggedFormat=DXGI_FORMAT_UNKNOWN;
            if(desc.Width!=loggedWidth || desc.Height!=loggedHeight || desc.Format!=loggedFormat) {
                SKSE::log::info("Wheel render target: {} {}x{} format={}; UI overlay enabled",
                    engineTarget?"engine UI framebuffer":"swap-chain fallback",desc.Width,desc.Height,static_cast<int>(desc.Format));
                loggedWidth=desc.Width;loggedHeight=desc.Height;loggedFormat=desc.Format;
            }
            auto& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
            SetViewport(io.DisplaySize.x,io.DisplaySize.y);
            const auto fontStarted=std::chrono::steady_clock::now();
            if (PrepareFonts(view, ViewScale(io.DisplaySize.x, io.DisplaySize.y, view))) {
                ImGui_ImplDX11_InvalidateDeviceObjects();
                if (!ImGui_ImplDX11_CreateDeviceObjects()) throw std::runtime_error("Unable to upload wheel font atlas");
                SKSE::log::info("Native-size font atlas: output={}x{}, scale={}, atlas={}x{}, visible_items={}, build_upload_ms={:.2f}",
                    desc.Width, desc.Height, Config().scale, io.Fonts->TexWidth, io.Fonts->TexHeight,view.items.size(),
                    std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fontStarted).count());
            }
            const auto now = std::chrono::steady_clock::now();
            io.DeltaTime = std::clamp(std::chrono::duration<float>(now - lastFrame).count(), .001f, .1f);
            lastFrame = now;
            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();
            DrawWheel(view,opacity,transition.value);
            ImGui::Render();
            // ImGui preserves its pipeline state; the output-merger targets are restored here.
            ID3D11RenderTargetView* previousTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
            ID3D11DepthStencilView* previousDepth = nullptr;
            context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previousTargets, &previousDepth);
            auto rawTarget = target.Get();
            context->OMSetRenderTargets(1, &rawTarget, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, previousTargets, previousDepth);
            for (auto previous : previousTargets) if (previous) previous->Release();
            if (previousDepth) previousDepth->Release();
            // No backbuffer reference survives Present, so ResizeBuffers can run normally.
        }

        HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* chain, UINT interval, UINT flags) {
            if (chain == targetSwapChain && !(flags & DXGI_PRESENT_TEST) && !failed) {
                auto previousContext = ImGui::GetCurrentContext();
                try {
                    if (imgui) ImGui::SetCurrentContext(imgui);
                    if (!ready && !Initialize(chain)) {
                        failed = true;
                        SKSE::log::error("D3D11 initialization failed; retaining vanilla menu");
                    }
                    if (ready && (IsOpen() || fading)) Render(chain);
                } catch (const std::exception& e) {
                    ready = false; failed = true;
                    SKSE::log::error("Wheel rendering disabled: {}", e.what());
                    if (auto tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Cancel(); });
                }
                ImGui::SetCurrentContext(previousContext);
            }
            return previousPresent(chain, interval, flags);
        }
    }
    bool RendererReady() { return ready; }
    bool InstallRenderer() {
        auto renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (!renderer) return false;
        targetSwapChain = reinterpret_cast<IDXGISwapChain*>(renderer->GetRuntimeData().renderWindows[0].swapChain);
        if (!targetSwapChain) { SKSE::log::error("No game swap chain at DataLoaded"); return false; }
        auto vtable = *reinterpret_cast<std::uintptr_t**>(targetSwapChain);
        previousPresent = reinterpret_cast<Present>(vtable[8]);
        REL::safe_write(reinterpret_cast<std::uintptr_t>(&vtable[8]), reinterpret_cast<std::uintptr_t>(&PresentHook));
        SKSE::log::info("Swap-chain Present hook installed");
        return true;
    }
}

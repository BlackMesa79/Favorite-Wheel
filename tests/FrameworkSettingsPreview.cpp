// 使用独立 ImGui 上下文和 WARP 渲染生产页面，不加载游戏或框架 DLL。
#include <string>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
namespace SKSE::log {
    template<class... T> void info(const char*,T&&...) {}
    template<class... T> void warn(const char*,T&&...) {}
    template<class... T> void error(const char* text,T&&...) {std::cerr<<text<<'\n';std::exit(2);}
}
#include "../src/FrameworkSettings.cpp"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
namespace Wheel {bool IsOpen(){return false;}}
namespace {
    using namespace Wheel::FrameworkSettings;
    void Require(bool value){if(!value)throw std::runtime_error("framework preview check failed");}
    void Check(HRESULT result){Require(SUCCEEDED(result));}
    void Adapter() {
        api.text=ImGui::TextUnformatted;api.check=ImGui::Checkbox;
        api.slider=[](const char* l,int* v,int a,int b,const char* f,int flags){return ImGui::SliderInt(l,v,a,b,f,flags);};
        api.sliderFloat=[](const char* l,float* v,float a,float b,const char* f,int flags){return ImGui::SliderFloat(l,v,a,b,f,flags);};
        api.button=[](const char* l,Vec2 s){return ImGui::Button(l,{s.x,s.y});};
        api.sameLine=ImGui::SameLine;api.separatorText=ImGui::SeparatorText;api.spacing=ImGui::Spacing;
        api.pushWrap=ImGui::PushTextWrapPos;api.popWrap=ImGui::PopTextWrapPos;api.nextWidth=ImGui::SetNextItemWidth;
        api.alignText=ImGui::AlignTextToFramePadding;api.available=[](Vec2* v){const auto a=ImGui::GetContentRegionAvail();*v={a.x,a.y};};
        api.fontSize=ImGui::GetFontSize;
        api.beginTable=[](const char* l,int n,int flags,Vec2 s,float w){return ImGui::BeginTable(l,n,flags,{s.x,s.y},w);};
        api.endTable=ImGui::EndTable;api.nextRow=ImGui::TableNextRow;api.nextColumn=ImGui::TableNextColumn;api.setupColumn=ImGui::TableSetupColumn;
        api.beginDisabled=ImGui::BeginDisabled;api.endDisabled=ImGui::EndDisabled;
        api.beginCombo=ImGui::BeginCombo;api.endCombo=ImGui::EndCombo;
        api.select=[](const char* l,bool selected,int flags,Vec2 s){return ImGui::Selectable(l,selected,flags,{s.x,s.y});};
    }
    void SaveImage(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11Texture2D* texture,
        D3D11_TEXTURE2D_DESC desc,const std::string& path) {
        using Microsoft::WRL::ComPtr;
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;Check(device->CreateTexture2D(&desc,nullptr,&staging));context->CopyResource(staging.Get(),texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};Check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
        BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);info.biPlanes=1;info.biBitCount=32;
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&file),sizeof(file));out.write(reinterpret_cast<char*>(&info),sizeof(info));
        std::vector<unsigned char> row(desc.Width*4);
        for(unsigned y=0;y<desc.Height;++y) {
            const auto pixels=static_cast<const unsigned char*>(mapped.pData)+y*mapped.RowPitch;
            for(unsigned x=0;x<desc.Width;++x){row[x*4]=pixels[x*4+2];row[x*4+1]=pixels[x*4+1];row[x*4+2]=pixels[x*4];row[x*4+3]=255;}
            out.write(reinterpret_cast<char*>(row.data()),row.size());
        }
        context->Unmap(staging.Get(),0);Require(bool(out));
    }
}
int main(int argc,char** argv) {
    using namespace Wheel;using namespace Wheel::FrameworkSettings;using Microsoft::WRL::ComPtr;
    LoadResources("assets");
    const auto root=std::filesystem::path("build")/("framework-preview-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);SetSettingsPath((root/"settings.ini").string());LoadSettings();
    auto config=Config();config.language=argc>1?argv[1]:"zh_CN";EditSettings(config);Refresh();
    // 跨页草稿、当前页重置和未暴露字段保护。
    draft.hotkey=44;draft.positionX=20;draft.gamepadHotkey=268;draft.enabled=false;draft.font="preserved.ttf";
    Defaults(Page::Appearance);Require(draft.positionX==Settings{}.positionX && draft.hotkey==44 && draft.gamepadHotkey==268);
    Require(!draft.enabled && draft.font=="preserved.ttf");Defaults(Page::Keyboard);Require(draft.hotkey==Settings{}.hotkey && draft.gamepadHotkey==268);
    draft.kbMoveWhileOpen=true;draft.kbCategoryPrevKey=46;draft.kbCategoryNextKey=44;
    Defaults(Page::Controller);Require(draft.kbMoveWhileOpen && draft.kbCategoryPrevKey==46 && draft.kbCategoryNextKey==44);
    Defaults(Page::Gameplay);Require(!draft.kbMoveWhileOpen && draft.kbCategoryPrevKey==44 && draft.kbCategoryNextKey==46);
    Refresh();
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    D3D11_TEXTURE2D_DESC desc{};desc.Width=1100;desc.Height=760;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;Check(device->CreateTexture2D(&desc,nullptr,&texture));
    ComPtr<ID3D11RenderTargetView> target;Check(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
    auto raw=target.Get();context->OMSetRenderTargets(1,&raw,nullptr);
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1100,760};io.DeltaTime=1.f/60;
    ImFontGlyphRangesBuilder glyphBuilder;glyphBuilder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    glyphBuilder.AddChar(0x2190);glyphBuilder.AddChar(0x2192); // Category direction labels.
    glyphBuilder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());ImVector<ImWchar> glyphs;glyphBuilder.BuildRanges(&glyphs);
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc",20,nullptr,glyphs.Data);
    ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.FramePadding={9,5};style.ItemSpacing={10,8};style.CellPadding={8,4};
    Require(ImGui_ImplDX11_Init(device.Get(),context.Get()));Adapter();
    for(int index=0;index<4;++index) {
        for(int frame=0;frame<3;++frame) {
            ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
            ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Mod Control Panel (offscreen preview)",nullptr,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoCollapse);
            ImGui::BeginChild("sidebar",{220,0},true);ImGui::TextUnformatted("FavoriteWheel");
            for(int p=0;p<4;++p)ImGui::Selectable(Tr(config,PageKey(static_cast<Page>(p))).c_str(),p==index);
            ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("page");
            RenderPage(static_cast<Page>(index));
            Require(ImGui::GetScrollMaxX()==0 && ImGui::GetScrollMaxY()==0);
            ImGui::EndChild();ImGui::End();ImGui::Render();
            const float clear[]={.06f,.065f,.07f,1};context->ClearRenderTargetView(target.Get(),clear);
            const auto data=ImGui::GetDrawData();Require(data->TotalVtxCount>100);
            for(int list=0;list<data->CmdListsCount;++list)for(const auto& v:data->CmdLists[list]->VtxBuffer)Require(std::isfinite(v.pos.x) && std::isfinite(v.pos.y));
            ImGui_ImplDX11_RenderDrawData(data);
        }
        SaveImage(device.Get(),context.Get(),texture.Get(),desc,(root/("page-"+std::to_string(index)+".bmp")).string());
    }
    ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();std::cout<<"Four production pages rendered; page-default checks passed: "<<root.string()<<'\n';
}

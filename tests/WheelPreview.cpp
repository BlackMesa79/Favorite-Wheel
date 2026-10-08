// Offscreen visual QA: uses the actual wheel drawing code, a WARP D3D11 device,
// and synthetic inventory data. It does not launch or interact with Skyrim.
#include "Wheel.h"
#include "Settings.h"
#include "WheelFonts.h"
#include "UIResources.h"
#include "WheelIcons.h"
#include "Transition.h"
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <cstdlib>
namespace Wheel {
    View preview;
    View Snapshot() { return preview; }
}
void Check(HRESULT hr) { if (FAILED(hr)) { std::cerr << "D3D11 failure: " << std::hex << hr; std::exit(1); } }
int main(int argc, char** argv) {
    using Microsoft::WRL::ComPtr;
    unsigned width = argc > 2 ? std::atoi(argv[2]) : 1600;
    unsigned height = argc > 3 ? std::atoi(argv[3]) : 1000;
    const bool empty = argc > 4 && std::string(argv[4]) == "empty";
    Wheel::LoadResources("assets");
    auto config=Wheel::Config();
    if(argc>5)config.language=argv[5];
    if(argc>6)config.theme=argv[6];
    if(argc>7)config.wheelScale=std::atof(argv[7]);
    if(argc>8)config.overlayOpacity=std::atoi(argv[8]);
    if(argc>9)config.positionX=std::atoi(argv[9]);
    if(argc>10)config.positionY=std::atoi(argv[10]);
    if(argc>4 && (std::string(argv[4])=="inventory" || std::string(argv[4])=="inventory-controls" || std::string(argv[4])=="gameplay"))config.allInventory=true;
    if(argc>4 && std::string(argv[4])=="gameplay"){config.timeMode=1;config.slowPercent=20;}
    Wheel::EditSettings(config);
    Wheel::preview.config=config;
    Wheel::preview.settingsOpen=argc>4 && std::string(argv[4])=="settings";
    if(argc>4 && (std::string(argv[4])=="controls" || std::string(argv[4])=="controls-pad" || std::string(argv[4])=="inventory-controls" || std::string(argv[4])=="gameplay")) {
        Wheel::preview.settingsOpen=true;Wheel::preview.settingsTab=(std::string(argv[4])=="inventory-controls" || std::string(argv[4])=="gameplay")?2:1;
    }
    Wheel::preview.gamepad=argc>4 && (std::string(argv[4])=="pad" || std::string(argv[4])=="controls-pad");
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;
    Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=width; desc.Height=height; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_DEFAULT; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;
    Check(device->CreateTexture2D(&desc,nullptr,&texture));
    ComPtr<ID3D11RenderTargetView> target;
    Check(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
    auto raw = target.Get(); context->OMSetRenderTargets(1,&raw,nullptr);
    // Synthetic textured scene exposes unwanted transparency and background changes.
    std::vector<unsigned char> scene(width*height*4);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const int detail=((x/8+y/8)%2)*48;
        const auto at=(y*width+x)*4;
        scene[at]=40+detail;scene[at+1]=52+detail;scene[at+2]=65+detail;scene[at+3]=255;
    }
    context->UpdateSubresource(texture.Get(),0,nullptr,scene.data(),width*4,0);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize={float(width),float(height)}; io.DeltaTime=1.0f/60;
    if (!ImGui_ImplDX11_Init(device.Get(),context.Get())) return 3;
    Wheel::preview.open=true; Wheel::preview.category=Wheel::Category::Armor;
    Wheel::preview.x=.62f; Wheel::preview.y=-.2f;
    if (!empty) {
        const char* names[]={"乌木铠甲（传奇）","龙鳞头盔","附魔龙骨重甲靴","夜莺手套","阿祖拉的守护护符","银制蓝宝石戒指","魔抗乌木盾","龙鳞护腕","法术消耗降低的精致法袍","铁制头盔"};
        constexpr Wheel::IconKind icons[]={Wheel::IconKind::Armor,Wheel::IconKind::Helmet,Wheel::IconKind::Boots,Wheel::IconKind::Gloves,Wheel::IconKind::Amulet,Wheel::IconKind::Ring,Wheel::IconKind::Shield,Wheel::IconKind::Gloves,Wheel::IconKind::Robe,Wheel::IconKind::Helmet};
        for(int i=0;i<23;++i) Wheel::preview.items.push_back({{},Wheel::Category::Armor,names[i%10],1,i==2,false,true,Wheel::ActionKind::Favorite,0,icons[i%10]});
        for(int i=0;i<8;++i)Wheel::preview.items[i].quickSlot=i;
    }
    if(argc>4 && (std::string(argv[4])=="functions" || std::string(argv[4])=="functions-no-light" || std::string(argv[4])=="outfit-missing" || std::string(argv[4])=="manage" || std::string(argv[4])=="name" || std::string(argv[4])=="name-long" || std::string(argv[4])=="confirm-delete" || std::string(argv[4])=="confirm-overwrite")) {
        Wheel::preview.functions=true;
        Wheel::preview.faceLightAvailable=std::string(argv[4])!="functions-no-light";
        Wheel::preview.items={
            {{},Wheel::Category::Armor,Wheel::Tr(config,"outfitSave"),0,false,true,true,Wheel::ActionKind::SaveOutfit},
            {{},Wheel::Category::Armor,Wheel::Tr(config,"outfitImport"),0,false,true,true,Wheel::ActionKind::ImportOutfits},
            {{},Wheel::Category::Armor,"城镇便装",5,true,false,true,Wheel::ActionKind::Outfit,1},
            {{},Wheel::Category::Armor,"冒险重甲",7,false,false,true,Wheel::ActionKind::Outfit,2},
            {{},Wheel::Category::Armor,"物品缺失的旅行套装",4,false,false,false,Wheel::ActionKind::Outfit,3}};
        if(std::string(argv[4])=="outfit-missing") {
            auto& item=Wheel::preview.items[2];item.equipped=false;item.usable=false;
            item.detail=Wheel::Tr(config,"outfitItemChanged")+": "+
                (config.language=="zh_CN"?"破碎皇家护甲 · 黑檀色左肩与腰部装饰配件（传奇）":"Shattered Royal Armor - Ebony left shoulder and waist accessory (Legendary)");
        } else if(std::string(argv[4])!="functions" && std::string(argv[4])!="functions-no-light") {
            Wheel::preview.outfitDialog=std::string(argv[4])=="manage"?3:1;
            if(std::string(argv[4])=="confirm-delete")Wheel::preview.outfitDialog=5;
            if(std::string(argv[4])=="confirm-overwrite")Wheel::preview.outfitDialog=4;
            Wheel::preview.outfitName.Set("城镇便装");
            if(std::string(argv[4])=="name-long") {
                Wheel::preview.outfitName.Set(std::string(100,'W')+"旅行套装名称测试");
                Wheel::preview.outfitName.Move(-1,true);Wheel::preview.outfitName.Move(-1,true);
            }
        }
    }
    if(argc>4 && (std::string(argv[4])=="lighting" || std::string(argv[4])=="followers")) {
        using namespace Wheel;
        preview.functions=true;
        preview.faceLightAvailable=true;
        preview.functionSection=std::string(argv[4])=="followers"?FaceLight::Section::Followers:FaceLight::Section::Lighting;
        auto item=[&](const std::string& name,IconKind icon,bool enabled,bool usable,const std::string& detail,ActionKind action=ActionKind::FaceLightCommand) {
            Item row;row.name=name;row.category=Category::Other;row.icon=icon;row.equipped=enabled;row.usable=usable;row.magic=true;row.detail=detail;row.action=action;return row;
        };
        if(preview.functionSection==FaceLight::Section::Lighting)preview.items={
            item(Tr(config,"lightPlayer"),IconKind::LightPlayer,true,true,Tr(config,"lightAmbient")),
            item(Tr(config,"lightTarget")+" · 莱迪亚",IconKind::LightTarget,false,true,Tr(config,"lightPreferenceHint")),
            item(Tr(config,"lightFollowerGroup"),IconKind::LightGroup,true,true,Tr(config,"lightGroupHint")),
            item(Tr(config,"lightFollowers"),IconKind::LightGroup,false,true,Tr(config,"lightOpen"),ActionKind::FaceLightFollowers)};
        else preview.items={
            item("莱迪亚",IconKind::LightTarget,true,true,Tr(config,"lightSneak")),
            item("瑟拉娜",IconKind::LightTarget,false,true,Tr(config,"lightPreferenceHint")),
            item("旅行伙伴 · 未加载",IconKind::LightTarget,true,false,Tr(config,"lightUnloaded")),
            item(Tr(config,"lightBack"),IconKind::Back,false,true,"",ActionKind::FunctionBack)};
    }
    if(argc>4 && (std::string(argv[4])=="spells" || std::string(argv[4])=="shouts" || std::string(argv[4])=="powers")) {
        const std::string mode=argv[4];
        Wheel::preview.category=mode=="spells"?Wheel::Category::Spells:mode=="shouts"?Wheel::Category::Shouts:Wheel::Category::Powers;
        const bool chinese=config.language=="zh_CN";
        const char* names=mode=="spells"?(chinese?"火焰术":"Flames"):mode=="shouts"?(chinese?"不卸之力":"Unrelenting Force"):(chinese?"夜视":"Night Eye");
        for(auto& item:Wheel::preview.items){item.category=Wheel::preview.category;item.name=names;item.magic=true;item.icon=Wheel::IconKind::Auto;item.count=1;}
        Wheel::preview.gamepad=mode=="shouts";
    }
    if(argc>4 && std::string(argv[4])=="pages" && !Wheel::preview.items.empty()) {
        const auto sample=Wheel::preview.items[0];
        Wheel::preview.items.resize(103,sample);Wheel::preview.page=5;
    }
    if(argc>4 && std::string(argv[4])=="sparse" && !Wheel::preview.items.empty())Wheel::preview.items.resize(1);
    if(argc>4 && std::string(argv[4])=="inventory") {
        Wheel::preview.inventoryWide=true;Wheel::preview.items.resize(10);
        Wheel::preview.totalItems=10003;Wheel::preview.page=523;Wheel::preview.itemOffset=5230;
        for(auto& item:Wheel::preview.items){item.quickSlot=-1;item.favorited=false;item.inventoryWide=true;}
    }
    if(argc>4 && std::string(argv[4])=="no-hints") {
        config.showHints=false;Wheel::preview.config.showHints=false;
    }
    if(argc>4 && std::string(argv[4])=="no-animation") {
        config.animations=false;Wheel::preview.config.animations=false;
    }
    if(argc>4 && std::string(argv[4])=="details" && !Wheel::preview.items.empty()) {
        auto& item=Wheel::preview.items[2];item.name="A very long custom armor name / 旅行套装与精致法袍的完整名称测试";
        item.detail="Long status description: 骨骼缩放与自定义说明文字 · Close the wheel to resume the game. This extra sentence must remain inside the card.";
    }
    if(argc>4 && !Wheel::preview.items.empty()) {
        const std::string mode=argv[4];
        if(mode=="info-armor" || mode=="info-potion" || mode=="info-spell" || mode=="info-many") {
            auto& item=Wheel::preview.items[2];item.info.weight=.5f;item.info.value=125;
            if(mode=="info-armor") {
                item.info.weight=8.5f;item.info.value=1685;item.info.armor=54.3f;item.info.enchantment=true;
                Wheel::AddEffect(item.info,"提高生命上限",35,0,0,false,false,false,false);
                Wheel::AddEffect(item.info,"抵抗冰霜",25,0,0,false,false,false,false);
            } else if(mode=="info-spell") {
                Wheel::preview.category=Wheel::Category::Spells;item.name="火焰术";item.magic=true;item.icon=Wheel::IconKind::Magic;
                item.info={};item.info.magicka=6.5f;item.info.costPerSecond=true;
                Wheel::AddEffect(item.info,"火焰伤害",8,1,0,false,false,false,false);
            } else {
                Wheel::preview.category=Wheel::Category::Potions;item.name="治疗与耐力恢复药水";item.icon=Wheel::IconKind::Potion;item.equipped=false;
                Wheel::AddEffect(item.info,"恢复生命",50,0,0,false,false,false,false);
                Wheel::AddEffect(item.info,"恢复耐力",25,0,0,false,false,false,false);
                if(mode=="info-many")for(int i=0;i<8;++i)Wheel::AddEffect(item.info,"很长的效果名称 / Fortify stamina and resist elemental damage",5,300,20,false,false,false,false);
            }
        }
    }
    const auto scale = Wheel::ViewScale(float(width),float(height),Wheel::preview);
    auto require = [](bool ok) { if (!ok) { std::cerr << "Visual regression failed\n"; std::exit(5); } };
    for(const auto viewport : {ImVec2{1280,720},ImVec2{2560,1440},ImVec2{3440,1440}})
        for(int x : {0,28,100}) for(int y : {0,46,100}) for(float size : {.6f,1.f,1.5f}) {
            auto layout=Wheel::preview;layout.settingsOpen=false;layout.outfitDialog=0;
            layout.config.positionX=x;layout.config.positionY=y;layout.config.wheelScale=size;
            const auto s=Wheel::ViewScale(viewport.x,viewport.y,layout);
            const auto c=Wheel::ViewCentre(viewport.x,viewport.y,layout);
            require(c.x-405*s>=-.01f && c.x+405*s<=viewport.x+.01f &&
                c.y-410*s>=-.01f && c.y+410*s<=viewport.y+.01f);
        }
    if(!Wheel::preview.settingsOpen && !Wheel::preview.outfitDialog) {
        const auto centre=Wheel::ViewCentre(float(width),float(height),Wheel::preview);
        require(centre.x-390*scale>=0 && centre.x+390*scale<=width &&
            centre.y-405*scale>=0 && centre.y+405*scale<=height);
    }
    require(Wheel::PrepareFonts(Wheel::preview, scale));
    const auto& visual=Wheel::Style(config);
    require(Wheel::FontAt(25*scale*visual.titleScale)->FontSize==std::round(25*scale*visual.titleScale));
    require(Wheel::FontAt(16*scale*visual.labelScale)->FontSize==std::round(16*scale*visual.labelScale));
    require(!Wheel::PrepareFonts(Wheel::preview, scale));
    auto changed = Wheel::preview;
    changed.items.push_back({{},Wheel::Category::Armor,"麟",1,false,false,true});
    require(Wheel::PrepareFonts(changed, scale));
    for (int size : {12,14,15,16,17,18,20,21,22,25,30}) {
        auto font = Wheel::FontAt(size * scale);
        require(font->FontSize == std::round(size * scale) && font->FindGlyphNoFallback(L'麟'));
    }
    require(Wheel::PrepareFonts(Wheel::preview, scale * .75f));
    require(Wheel::PrepareFonts(Wheel::preview, scale));
    auto warmed=Wheel::preview;warmed.inventoryGlyphs="龘";
    require(Wheel::PrepareFonts(warmed,scale));
    warmed.items.push_back({{},Wheel::Category::Armor,"龘",1,false,false,true});
    require(!Wheel::PrepareFonts(warmed,scale));
    auto effectGlyph=Wheel::preview;
    Wheel::AddEffect(effectGlyph.items.emplace_back().info,"霽",1,0,0,false,false,false,false);
    require(Wheel::PrepareFonts(effectGlyph,scale));
    require(Wheel::FontAt(14*scale)->FindGlyphNoFallback(L'霽')!=nullptr);
    require(!Wheel::PrepareFonts(effectGlyph,scale));
    std::cout << "Font native sizes, glyph expansion, reuse and resize passed; atlas " << io.Fonts->TexWidth << 'x' << io.Fonts->TexHeight << '\n';
    // Optional last argument captures the same render-only pose in either direction.
    const float expansion=argc>11?std::clamp(std::stof(argv[11]),0.f,1.f):1.f;
    const float opacity=Wheel::preview.config.animations?Wheel::TransitionOpacity(expansion):1.f;
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame(); Wheel::DrawWheel(Wheel::preview,opacity,expansion);
    if(argc>4 && std::string(argv[4])=="icons") {
        auto d=ImGui::GetBackgroundDrawList();
        const float sheetScale=height/1440.f;
        d->AddRectFilled({width*.53f,60*sheetScale},{width-30*sheetScale,height-60*sheetScale},IM_COL32(18,23,29,255),16*sheetScale);
        auto titleFont=Wheel::FontAt(24*sheetScale);
        d->AddText(titleFont,24*sheetScale,{width*.55f,92*sheetScale},IM_COL32(229,207,159,255),"N O R T H   E T C H  /  2 9");
        const char* labels[]={"Sword","Dagger","Axe","Mace","Bow","Crossbow","Staff","Arrow","Armor","Robe","Helmet","Gloves","Boots","Ring","Amulet","Shield","Potion","Food","Magic","Scroll","Torch","Other","Save outfit","Import","Outfit","Player light","Target light","Group light","Back"};
        const float left=width*.55f,cell=(width*.43f-40*sheetScale)/5,top=height*.18f,step=height*.135f;
        for(int i=0;i<29;++i) {
            Wheel::Item item;
            if(i<22)item.icon=static_cast<Wheel::IconKind>(i+1);
            else if(i<25)item.action=i==22?Wheel::ActionKind::SaveOutfit:i==23?Wheel::ActionKind::ImportOutfits:Wheel::ActionKind::Outfit;
            else item.icon=static_cast<Wheel::IconKind>(static_cast<int>(Wheel::IconKind::LightPlayer)+i-25);
            const ImVec2 at{left+(i%5+.5f)*cell,top+(i/5)*step};
            Wheel::DrawIcon(d,at,item,1.85f*sheetScale,IM_COL32(229,207,159,255));
            const float labelSize=20*sheetScale;
            auto font=Wheel::FontAt(labelSize);auto size=font->CalcTextSizeA(labelSize,FLT_MAX,0,labels[i]);
            d->AddText(font,labelSize,{at.x-size.x/2,at.y+52*sheetScale},IM_COL32(173,180,187,255),labels[i]);
        }
    }
    ImGui::Render();
    for(int list=0;list<ImGui::GetDrawData()->CmdListsCount;++list)
        for(const auto& vertex:ImGui::GetDrawData()->CmdLists[list]->VtxBuffer)
            require(std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y));
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    desc.Usage=D3D11_USAGE_STAGING; desc.BindFlags=0; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging; Check(device->CreateTexture2D(&desc,nullptr,&staging));
    context->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{}; Check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    const auto pixel=static_cast<const unsigned char*>(mapped.pData);
    for(int channel=0;channel<3;++channel) {
        const int dim=static_cast<int>((config.overlayOpacity*255/100)*opacity);
        require(std::abs(int(pixel[channel])-int(std::round(scene[channel]*(1.f-dim/255.f))))<=1);
    }
    if(expansion==1 && !Wheel::preview.settingsOpen && !Wheel::preview.outfitDialog) {
        const auto centre=Wheel::ViewCentre(float(width),float(height),Wheel::preview);
        const unsigned sx=static_cast<unsigned>(centre.x),sy=static_cast<unsigned>(centre.y-292*scale);
        const auto untouched=pixel+sy*mapped.RowPitch+sx*4;
        for(int channel=0;channel<3;++channel) require(std::abs(int(untouched[channel])-int(std::round(scene[(sy*width+sx)*4+channel]*(1.f-(config.overlayOpacity*255/100)/255.f))))<=1);
        const unsigned sectorX=static_cast<unsigned>(centre.x),sectorY=static_cast<unsigned>(centre.y-245*scale);
        const auto sector=pixel+sectorY*mapped.RowPitch+sectorX*4;
        require(sector[3]==255);
        // Material tint may change RGB. Verify opacity by drawing exactly the same
        // geometry over a different scene, rather than asserting an implementation color.
        const std::array<unsigned char,4> before{sector[0],sector[1],sector[2],sector[3]};
        context->Unmap(staging.Get(),0);
        const float bright[]={.95f,.05f,.8f,1.f};context->ClearRenderTargetView(target.Get(),bright);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        context->CopyResource(staging.Get(),texture.Get());
        Check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        const auto after=static_cast<const unsigned char*>(mapped.pData)+sectorY*mapped.RowPitch+sectorX*4;
        for(int channel=0;channel<4;++channel)require(after[channel]==before[channel]);
        context->Unmap(staging.Get(),0);
        context->UpdateSubresource(texture.Get(),0,nullptr,scene.data(),width*4,0);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        context->CopyResource(staging.Get(),texture.Get());
        Check(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    }
    std::cout<<"Background dimming verified; expansion="<<expansion<<" (solid interiors checked when settled)\n";
    BITMAPFILEHEADER file{}; file.bfType=0x4D42; file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER); file.bfSize=file.bfOffBits+width*height*4;
    BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=width; info.biHeight=-static_cast<LONG>(height); info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
    std::ofstream out(argc>1 ? argv[1] : "build/wheel-preview.bmp",std::ios::binary);
    out.write(reinterpret_cast<char*>(&file),sizeof(file)); out.write(reinterpret_cast<char*>(&info),sizeof(info));
    std::vector<unsigned char> row(width*4);
    for(unsigned y=0;y<height;++y) {
        auto pixels=static_cast<unsigned char*>(mapped.pData)+y*mapped.RowPitch;
        for(unsigned x=0;x<width;++x) { row[x*4]=pixels[x*4+2]; row[x*4+1]=pixels[x*4+1]; row[x*4+2]=pixels[x*4]; row[x*4+3]=255; }
        out.write(reinterpret_cast<char*>(row.data()),row.size());
    }
    context->Unmap(staging.Get(),0);
    ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
    if (!out) return 4;
    std::cout << "Rendered actual wheel UI at " << width << "x" << height << '\n';
}

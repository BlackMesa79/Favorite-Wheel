#include "UILayout.h"
// Offscreen visual QA: uses the actual wheel drawing code, a WARP D3D11 device,
// and synthetic inventory data. It does not launch or interact with Skyrim.
#include "Wheel.h"
#include "CombatPresets.h"
#include "CombatPresetModel.h"
#include "Settings.h"
#include "WheelFonts.h"
#include "UIResources.h"
#include "WheelIcons.h"
#include "SkinTextures.h"
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
#include <chrono>
#include <iomanip>
#include <source_location>
namespace Wheel {
    View preview;
    View Snapshot() { return preview; }
}
// Draw.cpp references the runtime preset label provider; previews use fixed
// labels because this target deliberately excludes SKSE-backed preset storage.
namespace Wheel::Combat {
    std::string SlotLabel(std::uint32_t, Slot slot) {
        switch (slot) {
        case Slot::Left: return "Left: Set";
        case Slot::Right: return "Right: Set";
        default: return "Voice: Set";
        }
    }
}
namespace Wheel::Outfits {
    std::string ModeLabel(std::uint32_t) { return "Full Outfit"; }
    std::string PartStateLabel(std::uint32_t, Part) { return "Head: Set"; }
    bool IsLoose(std::uint32_t) { return false; }
}
void Check(HRESULT hr) { if (FAILED(hr)) { std::cerr << "D3D11 failure: " << std::hex << hr; std::exit(1); } }
int main(int argc, char** argv) {
    using Microsoft::WRL::ComPtr;
    unsigned width = argc > 2 ? std::atoi(argv[2]) : 1600;
    unsigned height = argc > 3 ? std::atoi(argv[3]) : 1000;
    const bool empty = argc > 4 && std::string(argv[4]) == "empty";
    const bool showcase = argc > 4 && std::string(argv[4]) == "showcase";
    Wheel::LoadResources(argc>13?argv[13]:"assets"); // Optional alternate package root for fallback/style comparisons.
    auto config=Wheel::Config();
    if(argc>5)config.language=argv[5];
    if(argc>6)config.theme=argv[6];
    if(argc>7)config.wheelScale=std::atof(argv[7]);
    if(argc>8)config.overlayOpacity=std::atoi(argv[8]);
    if(argc>9)config.positionX=std::atoi(argv[9]);
    if(argc>10)config.positionY=std::atoi(argv[10]);
    if(argc>4 && std::string(argv[4])=="fallback-font")config.font="build/missing-primary-font.ttf";
    if(argc>4 && std::string(argv[4])=="custom-font")config.font="C:/Windows/Fonts/georgia.ttf";
    if(argc>4 && (std::string(argv[4])=="inventory" || std::string(argv[4])=="inventory-controls" || std::string(argv[4])=="gameplay"))config.allInventory=true;
    if(argc>4 && std::string(argv[4])=="gameplay"){config.timeMode=1;config.slowPercent=50;}
    if(argc>4 && std::string(argv[4])=="pad-triggers")config.gamepadCategoryButtons=1;
    if(argc>4 && (std::string(argv[4])=="controls-pad-move" || std::string(argv[4])=="pad-move" || std::string(argv[4])=="pad-move-pause")) {
        config.gamepadMoveWhileOpen=true;config.timeMode=std::string(argv[4])=="pad-move-pause"?0:1;
    }
    if(argc>14){config.kbMoveWhileOpen=true;config.kbCategoryPrevKey=std::atoi(argv[14]);}
    if(argc>15)config.kbCategoryNextKey=std::atoi(argv[15]);
    Wheel::EditSettings(config);
    Wheel::preview.config=config;Wheel::preview.appliedGamepadMove=config.gamepadMoveWhileOpen;
    Wheel::preview.settingsOpen=argc>4 && std::string(argv[4])=="settings";
    if(argc>4 && (std::string(argv[4])=="controls-swapped" || std::string(argv[4])=="controls" || std::string(argv[4])=="controls-pad" || std::string(argv[4])=="inventory-controls" || std::string(argv[4])=="gameplay")) {
        Wheel::preview.settingsOpen=true;Wheel::preview.settingsTab=std::string(argv[4])=="controls-pad"?3:(std::string(argv[4])=="inventory-controls" || std::string(argv[4])=="gameplay")?2:1;
    }
    if(argc>4 && (std::string(argv[4])=="controls-swapped" || std::string(argv[4])=="mouse-swapped")) { config.swapMouseHands=true;Wheel::preview.config=config; }
    Wheel::preview.gamepad=argc>4 && (std::string(argv[4])=="pad" || std::string(argv[4])=="pad-triggers" || std::string(argv[4])=="controls-pad");
    if(argc>4 && (std::string(argv[4])=="pending-pad-on" || std::string(argv[4])=="pending-pad-off")) {
        config.gamepadMoveWhileOpen=std::string(argv[4])=="pending-pad-on";
        Wheel::preview.config=config;Wheel::preview.appliedGamepadMove=!config.gamepadMoveWhileOpen;
        Wheel::preview.gamepad=Wheel::preview.settingsOpen=true;Wheel::preview.settingsTab=3;
    }
    if(argc>4 && (std::string(argv[4])=="controls-pad-move" || std::string(argv[4])=="pad-move" || std::string(argv[4])=="pad-move-pause")) {
        Wheel::preview.gamepad=true;
        if(std::string(argv[4])=="controls-pad-move") {Wheel::preview.settingsOpen=true;Wheel::preview.settingsTab=3;}
    }
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level;
    Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    const auto icons=Wheel::IconLibrary::Initialize(device.Get(),context.Get(),Wheel::Themes());
    for(const auto& warning:icons.warnings)std::cout<<warning<<'\n';
    const auto materials=Wheel::SkinTextures::Initialize(device.Get(),context.Get(),Wheel::Themes());
    std::cout<<"Skin materials: textures="<<materials.textures<<", texels="<<materials.texels<<'\n';
    for(const auto& warning:materials.warnings)std::cout<<warning<<'\n';
    if(!materials.warnings.empty() && !(argc>4 && std::string(argv[4])=="material-fallback"))return 5;
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
        if(showcase) {
            // Neutral presentation backdrop; diagnostic modes retain their checkerboard.
            const float dx=(float(x)/width-.45f)*1.25f,dy=(float(y)/height-.46f)*1.15f;
            const int glow=int(12*std::clamp(1.f-dx*dx-dy*dy,0.f,1.f));
            scene[at]=12+glow;scene[at+1]=15+glow;scene[at+2]=18+glow;
        } else {scene[at]=40+detail;scene[at+1]=52+detail;scene[at+2]=65+detail;}
        scene[at+3]=255;
    }
    context->UpdateSubresource(texture.Get(),0,nullptr,scene.data(),width*4,0);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize={float(width),float(height)}; io.DeltaTime=1.0f/60;
    if (!ImGui_ImplDX11_Init(device.Get(),context.Get())) return 3;
    Wheel::preview.open=true; Wheel::preview.category=Wheel::Category::Armor;
    if(argc>4 && (std::string(argv[4])=="sorting" || std::string(argv[4])=="sorting-pad")) {
        Wheel::preview.sortingEdit=true;Wheel::preview.sortingLabel=Wheel::Tr(config,"sortCustom");
        Wheel::preview.gamepad=std::string(argv[4])=="sorting-pad";
    }

    Wheel::preview.x=.62f; Wheel::preview.y=-.2f;
    if (!empty) {
        const char* names[]={"乌木铠甲（传奇）","龙鳞头盔","附魔龙骨重甲靴","夜莺手套","阿祖拉的守护护符","银制蓝宝石戒指","魔抗乌木盾","龙鳞护腕","法术消耗降低的精致法袍","铁制头盔"};
        constexpr Wheel::IconKind icons[]={Wheel::IconKind::Armor,Wheel::IconKind::Helmet,Wheel::IconKind::Boots,Wheel::IconKind::Gloves,Wheel::IconKind::Amulet,Wheel::IconKind::Ring,Wheel::IconKind::Shield,Wheel::IconKind::Gloves,Wheel::IconKind::Robe,Wheel::IconKind::Helmet};
        for(int i=0;i<23;++i) Wheel::preview.items.push_back({{},Wheel::Category::Armor,names[i%10],1,i==2,false,true,Wheel::ActionKind::Favorite,0,icons[i%10]});
        for(int i=0;i<8;++i)Wheel::preview.items[i].quickSlot=i;
    }
    if(showcase) {
        if(config.language!="en" || config.theme!="skyrim")return 5;
        Wheel::preview.x=.84f;Wheel::preview.y=-.49f; // Pointer in clear space above the selected icon.
        const char* names[]={"Ebony Armor (Legendary)","Dragonscale Helmet","Dragonplate Boots","Nightingale Gloves",
            "Amulet of Talos","Silver Sapphire Ring","Ebony Shield","Dragonscale Gauntlets","Archmage's Robes","Iron Helmet"};
        for(std::size_t i=0;i<Wheel::preview.items.size();++i)Wheel::preview.items[i].name=names[i%std::size(names)];
        auto& item=Wheel::preview.items[2];
        item.info.weight=8.f;item.info.value=1685;item.info.armor=27.f;item.info.enchantment=true;
        Wheel::AddEffect(item.info,"Fortify Health",35,0,0,false,false,false,false);
        Wheel::AddEffect(item.info,"Resist Frost",25,0,0,false,false,false,false);
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
    if(argc>4 && (std::string(argv[4])=="vampire" || std::string(argv[4])=="spells" || std::string(argv[4])=="shouts" || std::string(argv[4])=="powers")) {
        Wheel::preview.vampireRestricted=std::string(argv[4])=="vampire";
        const std::string mode=argv[4];
        Wheel::preview.category=mode=="spells"?Wheel::Category::Spells:mode=="shouts"?Wheel::Category::Shouts:Wheel::Category::Powers;
        const bool chinese=config.language=="zh_CN";
        const char* names=mode=="spells"?(chinese?"火焰术":"Flames"):mode=="shouts"?(chinese?"不卸之力":"Unrelenting Force"):(chinese?"夜视":"Night Eye");
        for(auto& item:Wheel::preview.items){item.category=Wheel::preview.category;item.name=names;item.magic=true;item.icon=Wheel::IconKind::Auto;item.count=1;}
        Wheel::preview.gamepad=mode=="shouts";
    }
    if(argc>4 && std::string(argv[4])=="powers-equipped") {
        // Reproduce the mixed Latin-name/CJK-status feedback in the upper-right blade.
        Wheel::preview.category=Wheel::Category::Powers;
        const char* names[]={"Horde","Horde: Follow","Horde: Passive","Horde: Summon","Horde: Wait","Tailor"};
        Wheel::preview.items.resize(std::size(names));
        for(std::size_t i=0;i<std::size(names);++i) {
            auto& item=Wheel::preview.items[i];
            item.category=Wheel::Category::Powers;item.name=names[i];item.magic=true;
            item.icon=Wheel::IconKind::Auto;item.count=1;item.equipped=i==1;item.quickSlot=-1;
        }
        Wheel::preview.x=.5f;Wheel::preview.y=-.68f;
    }
    if(argc>4 && std::string(argv[4]).starts_with("combat")) {
        const std::string mode=argv[4];const bool chinese=config.language=="zh_CN";
        Wheel::preview.functions=true;Wheel::preview.functionSection=Wheel::FaceLight::Section::Combat;
        Wheel::preview.faceLightAvailable=mode=="combat-lighting";Wheel::preview.items.clear();
        Wheel::Item save;save.action=Wheel::ActionKind::SaveCombat;save.category=Wheel::Category::Weapons;
        save.name=Wheel::Tr(config,"combatSave");save.magic=true;Wheel::preview.items.push_back(save);
        const char* english[]={"Spellsword","Dual Steel Swords","Dual Flames","Sword and Shield","Sword and Torch","Hunting Bow",
            "Empty Hands","Sword, Empty Left","Lightning Storm","Enchanted Twin Blades","Missing Weapon"};
        const char* chineseNames[]={"魔剑士","双持钢剑","双手火舌术","剑盾","持剑照明","狩猎弓","空手","持剑与左手空手","闪电风暴","附魔双刃","缺失武器"};
        const char* left[]={"Healing","Steel Sword","Flames","Steel Shield","Torch","","Empty","Empty","","Frost Steel Sword","Steel Sword"};
        const char* right[]={"Steel Sword","Steel Sword","Flames","Steel Sword","Steel Sword","Hunting Bow","Empty","Steel Sword","Lightning Storm","Fire Steel Sword","Missing Blade"};
        const char* leftCN[]={"治疗术","钢剑","火舌术","钢盾","火把","","空手","空手","","寒霜钢剑","钢剑"};
        const char* rightCN[]={"钢剑","钢剑","火舌术","钢剑","钢剑","狩猎弓","空手","钢剑","闪电风暴","烈焰钢剑","缺失的刀刃"};
        for(int i=0;i<int(std::size(english));++i) {
            Wheel::Item item;item.action=Wheel::ActionKind::Combat;item.actionId=i+1;item.category=Wheel::Category::Weapons;
            item.magic=true;item.name=chinese?chineseNames[i]:english[i];item.equipped=i==0;item.usable=i!=10;
            if(mode=="combat-hotkeys" && i<8)item.quickSlot=i;
            if(mode=="combat-membership" || mode=="combat-membership-all")
                item.centreNote=Wheel::Tr(config,"cycleDefault")+"\n"+(mode=="combat-membership-all"?"1 / 2 / 3 / 4 / 5 / 6 / 7 / 8":"1 / 3");
            const bool both=i==5 || i==8;
            item.detail=both?Wheel::Tr(config,"combatBoth")+": "+(chinese?rightCN[i]:right[i]):
                Wheel::Tr(config,"combatLeft")+": "+(chinese?leftCN[i]:left[i])+"\n"+Wheel::Tr(config,"combatRight")+": "+(chinese?rightCN[i]:right[i]);
            if(!item.usable)item.detail=Wheel::Tr(config,"combatRight")+": "+(chinese?rightCN[i]:right[i])+" — "+Wheel::Tr(config,"combatMissing");
            Wheel::preview.items.push_back(item);
        }
        Wheel::preview.x=.55f;Wheel::preview.y=-.75f;
        if(mode=="combat-missing"){Wheel::preview.items={save,Wheel::preview.items.back()};}
        if(mode=="combat-empty"){Wheel::preview.items={save,Wheel::preview.items[7]};}
        if(mode=="combat-name" || mode=="combat-manage" || mode=="combat-confirm") {
            Wheel::preview.combatDialog=true;Wheel::preview.outfitId=1;
            Wheel::preview.outfitDialog=mode=="combat-name"?1:mode=="combat-manage"?3:4;
            Wheel::preview.outfitName.Set(chinese?"魔剑士": "Spellsword");Wheel::preview.x=Wheel::preview.y=0;
        }
    }
    if(argc>4 && std::string(argv[4]).starts_with("cycles")) {
        using namespace Wheel;const std::string mode=argv[4];const bool chinese=config.language=="zh_CN";
        preview.functions=true;preview.functionSection=FaceLight::Section::Cycles;preview.items.clear();
        for(int i=0;i<8;++i) {
            Item item;item.action=ActionKind::Cycle;item.actionId=i+1;item.magic=true;item.count=i==0?3:0;item.usable=i==0;
            item.name=Tr(config,"cycleDefault")+" "+std::to_string(i+1);
            item.detail="Shift + "+std::to_string(i+1)+" · "+(i==0?"1/3":"0/0")+"\n"+Tr(config,"cycleUse")+"\n"+Tr(config,"cycleStorage");
            preview.items.push_back(item);
        }
        preview.x=0;preview.y=-.75f;
        if(mode=="cycles-list") {
            preview.functionSection=FaceLight::Section::CycleMembers;preview.items.clear();
            Item back;back.name=Tr(config,"cycleTitle");back.action=ActionKind::FunctionBack;preview.items.push_back(back);
            Item capture;capture.name=Tr(config,"cycleCapture");capture.action=ActionKind::CycleCapture;preview.items.push_back(capture);
            for(int i=0;i<3;++i) {
                Item item;item.name=i==0?"Dual Steel Swords":i==1?"Sword and Shield":"Dual Flames";
                item.action=ActionKind::CycleMember;item.actionId=1;item.actionIndex=i+1;item.magic=true;
                item.detail="Left hand: Steel Sword\nRight hand: Steel Sword\n"+Tr(config,"cycleMemberHint");preview.items.push_back(item);
            }
        }else if(mode!="cycles") {
            preview.outfitDialog=mode=="cycles-confirm"?5:mode=="cycles-rename"?2:3;
            preview.cycleDialog=true;preview.cycleMemberDialog=mode=="cycles-member";
            preview.cycleCaptureKey=mode=="cycles-capture";preview.cycleBinding="Ctrl + Shift + F";
            preview.outfitName.Set(preview.cycleMemberDialog?(chinese?"双持钢剑":"Dual Steel Swords"):preview.items[0].name);
            preview.x=preview.y=0;
        }
    }
    if(argc>4 && (std::string(argv[4])=="hands" || std::string(argv[4])=="hands-top" || std::string(argv[4])=="hands-spell")) {
        Wheel::preview.items.resize(Wheel::slots);Wheel::preview.category=Wheel::Category::Weapons;
        constexpr Wheel::HandEquip hands[]={Wheel::HandEquip::Dual,Wheel::HandEquip::Right,Wheel::HandEquip::Left,
            Wheel::HandEquip::TwoHanded,Wheel::HandEquip::Left,Wheel::HandEquip::Left,Wheel::HandEquip::None,
            Wheel::HandEquip::None,Wheel::HandEquip::Right,Wheel::HandEquip::TwoHanded};
        constexpr Wheel::IconKind icons[]={Wheel::IconKind::Magic,Wheel::IconKind::Sword,Wheel::IconKind::Dagger,
            Wheel::IconKind::Bow,Wheel::IconKind::Shield,Wheel::IconKind::Torch,Wheel::IconKind::Sword,
            Wheel::IconKind::Armor,Wheel::IconKind::Staff,Wheel::IconKind::Magic};
        const char* english[]={"Flames","Steel Sword","Steel Dagger","Hunting Bow","Steel Shield","Torch",
            "Steel Sword","Iron Armor","Staff of Sparks","Lightning Storm"};
        const char* chinese[]={"火舌术","钢剑","钢匕首","狩猎弓","钢盾","火把","钢剑","铁甲","电火花法杖","闪电风暴"};
        for(int i=0;i<Wheel::slots;++i) {
            auto& item=Wheel::preview.items[i];item={};item.category=Wheel::Category::Weapons;
            item.name=config.language=="zh_CN"?chinese[i]:english[i];item.icon=icons[i];item.count=1;
            item.magic=i==0 || i==9;item.equipped=i!=6;item.handEquip=hands[i];item.quickSlot=i<8?i:-1;
            item.infoReady=true;
        }
        Wheel::preview.x=.55f;Wheel::preview.y=-.75f;
        if(std::string(argv[4])=="hands-top"){Wheel::preview.x=0;Wheel::preview.y=-.9f;}
        if(std::string(argv[4])=="hands-spell"){Wheel::preview.x=-.55f;Wheel::preview.y=-.75f;}
    }
    if(argc>4 && (std::string(argv[4])=="equipped-all" || std::string(argv[4])=="equipped-top" ||
        std::string(argv[4])=="equipped-bottom" || std::string(argv[4])=="equipped-lower")) {
        Wheel::preview.items.resize(Wheel::slots);
        for(auto& item:Wheel::preview.items)item.equipped=true;
        if(std::string(argv[4])=="equipped-top") {Wheel::preview.x=0;Wheel::preview.y=-.9f;}
        if(std::string(argv[4])=="equipped-bottom") {Wheel::preview.x=0;Wheel::preview.y=.9f;}
        if(std::string(argv[4])=="equipped-lower") {Wheel::preview.x=.53f;Wheel::preview.y=.73f;}
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
        if(mode=="poisons") {
            Wheel::preview.category=Wheel::Category::Poisons;
            const char* names[]={"Weak Poison","Lingering Damage Health","强效持续伤害生命毒药","Paralysis Poison","Slow Poison"};
            for(std::size_t i=0;i<Wheel::preview.items.size();++i) {
                auto& item=Wheel::preview.items[i];item.category=Wheel::Category::Poisons;
                item.name=names[i%std::size(names)];item.icon=i==0?Wheel::IconKind::Auto:Wheel::IconKind::Poison;
                item.equipped=false;item.count=static_cast<int>(i)+1;
            }
            auto& item=Wheel::preview.items[2];item.info.weight=.5f;item.info.value=125;
            Wheel::AddEffect(item.info,"持续伤害生命",5,10,0,false,false,false,false);
        }
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
    if(argc>4) {
        const std::string mode=argv[4];
        if(mode=="types-sparse")Wheel::preview.visibleCategories={
            (1u<<int(Wheel::Category::Armor))|(1u<<int(Wheel::Category::Potions))|(1u<<int(Wheel::Category::Powers))};
        if(mode=="types-single")Wheel::preview.visibleCategories={1u<<int(Wheel::Category::Armor)};
        if(mode=="types-empty") {Wheel::preview.visibleCategories={0};Wheel::preview.items.clear();}
        if(mode=="types-unhidden") {Wheel::preview.visibleCategories=Wheel::CategoryVisibility({0},false);Wheel::preview.items.clear();}
    }
    if(argc>4 && std::string(argv[4])=="sorting") {
        const auto point=Wheel::ShapePoint(Wheel::Style(config).layout,.75f,2*2*std::numbers::pi_v<float>/Wheel::slots);
        Wheel::preview.x=point.x;Wheel::preview.y=point.y;
    }
    if(argc>4 && std::string(argv[4])=="numeric-sort") {
        Wheel::preview.sortingLabel=Wheel::Tr(config,"sortArmorDesc");
    }
    if(argc>4 && (std::string(argv[4])=="drag-sort" || std::string(argv[4])=="drag-sort-static" || std::string(argv[4])=="drag-drop" || std::string(argv[4])=="drag-settled")) {
        Wheel::preview.sortingEdit=true;Wheel::preview.sortingSelectedSlot=0;
        Wheel::preview.sortingLabel=Wheel::Tr(config,"sortCustom");
        Wheel::preview.dragItem=Wheel::preview.items.front();Wheel::preview.sortDrag.Begin(0,-.75f);
        const auto point=Wheel::ShapePoint(Wheel::Style(config).layout,.75f,3*2*std::numbers::pi_v<float>/Wheel::slots);
        Wheel::preview.x=point.x;Wheel::preview.y=point.y;Wheel::preview.sortDrag.Move(point.x,point.y);
        if(std::string(argv[4])=="drag-sort-static"){config.animations=false;Wheel::preview.config.animations=false;}
    }
    const auto scale = Wheel::ViewScale(float(width),float(height),Wheel::preview);
    auto require = [](bool ok, std::source_location at=std::source_location::current()) {
        if (!ok) { std::cerr << "Visual regression failed at line " << at.line() << '\n'; std::exit(5); }
    };
    for(const auto viewport : {ImVec2{1280,720},ImVec2{2560,1440},ImVec2{3440,1440}})
        for(int x : {0,28,100}) for(int y : {0,46,100}) for(float size : {.6f,1.f,1.5f}) {
            auto layout=Wheel::preview;layout.settingsOpen=false;layout.outfitDialog=0;
            layout.config.positionX=x;layout.config.positionY=y;layout.config.wheelScale=size;
            const auto s=Wheel::ViewScale(viewport.x,viewport.y,layout);
            const auto c=Wheel::ViewCentre(viewport.x,viewport.y,layout);
            const auto& skin=Wheel::Style(layout.config).layout;
            const float hx=Wheel::SkinHalfWidth(skin)*s,hy=Wheel::SkinHalfHeight(skin)*s;
            require(c.x-hx>=-.01f && c.x+hx<=viewport.x+.01f && c.y-hy>=-.01f && c.y+hy<=viewport.y+.01f);
        }
    if(!Wheel::preview.settingsOpen && !Wheel::preview.outfitDialog) {
        const auto centre=Wheel::ViewCentre(float(width),float(height),Wheel::preview);
        const auto& skin=Wheel::Style(Wheel::preview.config).layout;
        require(centre.x-Wheel::SkinHalfWidth(skin)*scale>=-.01f && centre.x+Wheel::SkinHalfWidth(skin)*scale<=width+.01f &&
            centre.y-Wheel::SkinHalfHeight(skin)*scale>=-.01f && centre.y+Wheel::SkinHalfHeight(skin)*scale<=height+.01f);
    }
    if(argc>4 && std::string(argv[4])=="startup-font") {
        Wheel::View warm;warm.config=config;
        const auto started=std::chrono::steady_clock::now();
        require(Wheel::PrepareFonts(warm,scale));
        require(ImGui_ImplDX11_CreateDeviceObjects());
        auto first=warm;first.open=true;
        Wheel::Item item;item.name=Wheel::UIGlyphs(config);first.items.push_back(item);
        require(!Wheel::PrepareFonts(first,scale));
        std::cout<<"Startup UI atlas uploaded; first open with covered glyphs requires no rebuild; warmup_ms="
            <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';
        ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();return 0;
    }
    if(argc>4 && std::string(argv[4])=="font-categories") {
        auto base=Wheel::preview;base.items.clear();base.inventoryGlyphs.clear();
        const char* names[]={"麟","霽","龘","齉","靐"};
        std::vector<Wheel::Item> catalog;
        for(std::size_t i=0;i<std::size(names);++i) {
            Wheel::Item item;item.name=names[i];item.category=static_cast<Wheel::Category>(i);
            catalog.push_back(std::move(item));
        }
        auto duplicate=catalog;duplicate.insert(duplicate.end(),catalog.begin(),catalog.end());
        require(Wheel::CatalogFontGlyphs(duplicate)==Wheel::CatalogFontGlyphs(catalog));
        for(const bool prewarm:{false,true}) {
            Wheel::PrepareFonts(base,scale*.75f);Wheel::PrepareFonts(base,scale);
            if(prewarm)base.inventoryGlyphs=Wheel::CatalogFontGlyphs(catalog);
            auto prepare=[&](const Wheel::View& view) {
                const auto started=std::chrono::steady_clock::now();
                const bool rebuilt=Wheel::PrepareFonts(view,scale);
                if(rebuilt) {
                    ImGui_ImplDX11_InvalidateDeviceObjects();
                    require(ImGui_ImplDX11_CreateDeviceObjects());
                }
                return std::pair{rebuilt,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()};
            };
            const auto warm=prepare(base);
            int rebuilds=0;double maxMs=0,totalMs=0;
            for(int pass=0;pass<3;++pass)for(const auto& item:catalog) {
                auto page=base;page.category=item.category;page.items={item};
                const auto [rebuilt,ms]=prepare(page);rebuilds+=rebuilt;maxMs=std::max(maxMs,ms);totalMs+=ms;
            }
            require(prewarm?rebuilds==0:rebuilds==std::size(names));
            std::cout<<"Category font trace: catalog_prewarm="<<prewarm<<" initial_prewarm_ms="<<warm.second
                <<" switches=15 font_rebuilds="<<rebuilds<<" total_switch_ms="<<totalMs<<" max_switch_ms="<<maxMs<<'\n';
        }
        ImGui_ImplDX11_Shutdown();ImGui::DestroyContext();return 0;
    }
    require(Wheel::PrepareFonts(Wheel::preview, scale));
    const auto& visual=Wheel::Style(config);
    if(argc>4 && std::string(argv[4])=="fallback-font") {
        require(!Wheel::PreparedFontPath().empty() && Wheel::PreparedFontPath()!=Wheel::FontPath(config));
        require(std::string(Wheel::FontAt(14*scale)->GetDebugName()).find("Proggy")==std::string::npos);
        std::cout<<"Missing font uses real system primary: "<<Wheel::PreparedFontPath()<<'\n';
    }
    if(argc>4 && std::string(argv[4])=="custom-font")require(Wheel::PreparedFontPath()==config.font);
    require(Wheel::FontAt(25*scale*visual.titleScale)->FontSize==std::round(25*scale*visual.titleScale));
    require(Wheel::FontAt(16*scale*visual.labelScale)->FontSize==std::round(16*scale*visual.labelScale));
    require(!Wheel::PrepareFonts(Wheel::preview, scale));
    ImFontGlyphRangesBuilder uiGlyphs;uiGlyphs.AddText(Wheel::UIGlyphs(Wheel::preview.config).c_str());
    ImVector<ImWchar> uiRanges;uiGlyphs.BuildRanges(&uiRanges);
    for(int range=0;uiRanges[range];range+=2)for(unsigned code=uiRanges[range];code<=uiRanges[range+1];++code)
        require(Wheel::FontAt(14*scale)->FindGlyphNoFallback(static_cast<ImWchar>(code))!=nullptr);
    std::cout<<"Active language UI and all language names have font coverage\n";
    auto changed = Wheel::preview;
    changed.items.push_back({{},Wheel::Category::Armor,"麟",1,false,false,true});
    require(Wheel::PrepareFonts(changed, scale));
    for (int size : {12,13,14,15,16,17,18,20,21,22,25,30}) {
        auto font = Wheel::FontAt(size * scale);
        require(font->FontSize == std::round(size * scale) && font->FindGlyphNoFallback(L'麟'));
    }
    require(Wheel::PrepareFonts(Wheel::preview, scale * .75f));
    require(Wheel::PrepareFonts(Wheel::preview, scale));
    auto warmed=Wheel::preview;warmed.inventoryGlyphs="龘";
    require(Wheel::PrepareFonts(warmed,scale));
    warmed.items.push_back({{},Wheel::Category::Armor,"龘",1,false,false,true});
    require(!Wheel::PrepareFonts(warmed,scale));
    auto functionsWarmed=Wheel::preview;
    functionsWarmed.inventoryGlyphs=Wheel::CatalogFontGlyphs({},"霜脉熔星稲妻");
    Wheel::PrepareFonts(functionsWarmed,scale);
    for(const char* name:{"霜脉","熔星","稲妻"}) {
        functionsWarmed.items={};Wheel::Item item;item.name=name;item.action=Wheel::ActionKind::Combat;
        functionsWarmed.items.push_back(item);functionsWarmed.functionSection=Wheel::FaceLight::Section::Combat;
        require(!Wheel::PrepareFonts(functionsWarmed,scale));
    }
    std::cout<<"Prewarmed combat/outfit name switches require no font rebuild\n";
    auto effectGlyph=Wheel::preview;
    Wheel::AddEffect(effectGlyph.items.emplace_back().info,"霽",1,0,0,false,false,false,false);
    require(Wheel::PrepareFonts(effectGlyph,scale));
    require(Wheel::FontAt(14*scale)->FindGlyphNoFallback(L'霽')!=nullptr);
    require(!Wheel::PrepareFonts(effectGlyph,scale));
    for(const auto& language:Wheel::Languages()) {
        auto localized=Wheel::preview;localized.config.language=language.id;
        Wheel::PrepareFonts(localized,scale);
        ImFontGlyphRangesBuilder needed;needed.AddText(Wheel::UIGlyphs(localized.config).c_str());
        ImVector<ImWchar> neededRanges;needed.BuildRanges(&neededRanges);
        for(int range=0;neededRanges[range];range+=2)for(unsigned code=neededRanges[range];code<=neededRanges[range+1];++code)
            require(Wheel::FontAt(14*scale)->FindGlyphNoFallback(static_cast<ImWchar>(code))!=nullptr);
        require(!Wheel::PrepareFonts(localized,scale));
    }
    Wheel::PrepareFonts(Wheel::preview,scale);
    require(!Wheel::PrepareFonts(Wheel::preview,scale));
    std::cout<<"Language switching and font fallback atlas reuse passed\n";
    std::cout << "Font native sizes, glyph expansion, reuse and resize passed; atlas " << io.Fonts->TexWidth << 'x' << io.Fonts->TexHeight << '\n';
    // Optional last argument captures the same render-only pose in either direction.
    const float expansion=argc>11?std::clamp(std::stof(argv[11]),0.f,1.f):1.f;
    const float opacity=Wheel::preview.config.animations?Wheel::TransitionOpacity(expansion):1.f;
    // CPU-only benchmark of the production drawing path. Excludes font building,
    // WARP rasterization and readback; no claim about in-game GPU/frame time.
    if(argc>14 && std::string(argv[14])=="benchmark") {
        for(const bool sweep:{false,true}) {
            constexpr int samples=1200;
            std::vector<double> times;times.reserve(samples);
            int maxVertices=0,maxIndices=0,maxCommands=0;
            Wheel::ResetVisualFeedback();
            for(int frame=0;frame<samples+60;++frame) {
                const float progress=sweep?float(frame%61)/60.f:1.f;
                ImGui_ImplDX11_NewFrame();ImGui::NewFrame();
                const auto started=std::chrono::steady_clock::now();
                Wheel::DrawWheel(Wheel::preview,Wheel::TransitionOpacity(progress),progress);
                ImGui::Render();
                const auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
                if(frame>=60)times.push_back(elapsed);
                const auto data=ImGui::GetDrawData();
                maxVertices=std::max(maxVertices,data->TotalVtxCount);
                maxIndices=std::max(maxIndices,data->TotalIdxCount);
                int commands=0;
                for(const auto list:data->CmdLists)for(const auto& command:list->CmdBuffer)
                    if(command.ElemCount && !command.UserCallback)++commands;
                maxCommands=std::max(maxCommands,commands);
            }
            std::sort(times.begin(),times.end());
            double total=0;for(const auto time:times)total+=time;
            std::cout<<std::fixed<<std::setprecision(2)<<"CPU draw "<<(sweep?"sweep":"settled")
                <<": mean_us="<<total/times.size()<<", p95_us="<<times[times.size()*95/100]
                <<", max_vertices="<<maxVertices<<", max_indices="<<maxIndices<<", max_commands="<<maxCommands<<'\n';
        }
        Wheel::ResetVisualFeedback();
    }
    // Optional frame count settles hover feedback without sleeping or touching game state.
    const std::string dragMode=argc>4?argv[4]:"";
    const int frames=argc>12?std::clamp(std::atoi(argv[12]),1,30):dragMode=="drag-drop"?15:dragMode=="drag-settled"?30:dragMode=="drag-sort"?20:1;
    for(int frame=0;frame<frames;++frame) {
        if(frame==12 && (dragMode=="drag-drop" || dragMode=="drag-settled")) {
            Wheel::preview.sortDrag.Cancel();Wheel::preview.dragItem.reset();Wheel::preview.sortingSelectedSlot=3;
            std::rotate(Wheel::preview.items.begin(),Wheel::preview.items.begin()+1,Wheel::preview.items.begin()+4);
        }
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame(); Wheel::DrawWheel(Wheel::preview,opacity,expansion);
        if(frame+1<frames)ImGui::Render();
    }
    if(argc>4 && std::string(argv[4])=="icons") {
        auto d=ImGui::GetBackgroundDrawList();
        const float sheetScale=height/1440.f;
        d->AddRectFilled({width*.53f,60*sheetScale},{width-30*sheetScale,height-60*sheetScale},IM_COL32(18,23,29,255),16*sheetScale);
        auto titleFont=Wheel::FontAt(24*sheetScale);
        d->AddText(titleFont,24*sheetScale,{width*.55f,92*sheetScale},IM_COL32(229,207,159,255),"N O R T H   E T C H  /  3 0");
        const char* labels[]={"Sword","Dagger","Axe","Mace","Bow","Crossbow","Staff","Arrow","Armor","Robe","Helmet","Gloves","Boots","Ring","Amulet","Shield","Potion","Food","Magic","Scroll","Torch","Other","Save outfit","Import","Outfit","Player light","Target light","Group light","Back","Poison"};
        const float left=width*.55f,cell=(width*.43f-40*sheetScale)/5,top=height*.18f,step=height*.135f;
        for(int i=0;i<std::size(labels);++i) {
            Wheel::Item item;
            if(i<22)item.icon=static_cast<Wheel::IconKind>(i+1);
            else if(i<25)item.action=i==22?Wheel::ActionKind::SaveOutfit:i==23?Wheel::ActionKind::ImportOutfits:Wheel::ActionKind::Outfit;
            else if(i<29)item.icon=static_cast<Wheel::IconKind>(static_cast<int>(Wheel::IconKind::LightPlayer)+i-25);
            else item.icon=Wheel::IconKind::Poison;
            const ImVec2 at{left+(i%5+.5f)*cell,top+(i/5)*step};
            Wheel::DrawIcon(d,at,item,1.85f*sheetScale,IM_COL32(229,207,159,255));
            const float labelSize=20*sheetScale;
            auto font=Wheel::FontAt(labelSize);auto size=font->CalcTextSizeA(labelSize,FLT_MAX,0,labels[i]);
            d->AddText(font,labelSize,{at.x-size.x/2,at.y+52*sheetScale},IM_COL32(173,180,187,255),labels[i]);
        }
    }
    if(argc>4 && std::string(argv[4])=="schools") {
        auto d=ImGui::GetForegroundDrawList();
        d->AddRectFilled({width*.05f,height*.08f},{width*.95f,height*.92f},IM_COL32(18,23,29,255));
        const char* labels[]={"Alteration","Conjuration","Destruction","Illusion","Restoration","Shout","Power","Book","Other","Healing override"};
        const auto& theme=Wheel::Style(config);
        for(int i=0;i<10;++i) {
            Wheel::Item item;item.icon=i==8?Wheel::IconKind::Other:static_cast<Wheel::IconKind>(static_cast<int>(Wheel::IconKind::Alteration)+i);
            if(i==9){item.icon=Wheel::IconKind::Restoration;item.iconForm=Wheel::IconLibrary::FormKey("Skyrim.esm",0x12FCC);}
            const ImVec2 at{width*(.14f+.18f*(i%5)),height*(.32f+.36f*(i/5))};
            Wheel::DrawIcon(d,at,item,1.5f,theme.accent,&theme);
            d->AddText({at.x-42,at.y+45},theme.text,labels[i]);
        }
    }
    ImGui::Render();
    const auto drawData=ImGui::GetDrawData();
    int drawCalls=0;
    for(int list=0;list<drawData->CmdListsCount;++list)
        for(const auto& command:drawData->CmdLists[list]->CmdBuffer)
            if(command.ElemCount && !command.UserCallback)++drawCalls;
    std::cout<<"UI geometry: lists="<<drawData->CmdListsCount<<", draw_commands="<<drawCalls
        <<", vertices="<<drawData->TotalVtxCount<<", triangles="<<drawData->TotalIdxCount/3<<'\n';
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
    if(expansion==1 && !Wheel::preview.settingsOpen && !Wheel::preview.outfitDialog && !(argc>4 && std::string(argv[4])=="schools")) {
        const auto centre=Wheel::ViewCentre(float(width),float(height),Wheel::preview);
        const auto& skin=Wheel::Style(config).layout;
        // Avoid the large central title's snapped one-pixel shadow at tiny scales.
        const unsigned sx=static_cast<unsigned>(centre.x+180*scale),sy=static_cast<unsigned>(centre.y-(292+skin.ExtraY())*scale);
        const auto untouched=pixel+sy*mapped.RowPitch+sx*4;
        for(int channel=0;channel<3;++channel) require(std::abs(int(untouched[channel])-int(std::round(scene[(sy*width+sx)*4+channel]*(1.f-(config.overlayOpacity*255/100)/255.f))))<=1);
        const auto interior=Wheel::ShapePoint(skin,245*scale,0);
        const unsigned sectorX=static_cast<unsigned>(centre.x+interior.x),sectorY=static_cast<unsigned>(centre.y+interior.y);
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
    Wheel::IconLibrary::Reset();Wheel::SkinTextures::Reset();ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext();
    if (!out) return 4;
    std::cout << "Rendered actual wheel UI at " << width << "x" << height << '\n';
}

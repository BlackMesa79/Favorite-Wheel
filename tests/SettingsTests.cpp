#include "Settings.h"
#include "UIResources.h"
#include "UILayout.h"
#include "Transition.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cmath>
void Check(bool condition,const char* message) { if(!condition){std::cerr<<message<<'\n';std::exit(1);} }
int wmain(int argc,wchar_t** argv) {
    using namespace Wheel;
    auto utf8=[](const std::filesystem::path& path){const auto text=path.u8string();return std::string(reinterpret_cast<const char*>(text.data()),text.size());};
    if(argc==3 && std::wstring_view(argv[1])==L"--verify-persisted") {
        SetSettingsPath(utf8(argv[2]));LoadSettings();const auto v=Config();
        Check(!v.keepOpen && v.gamepadCategoryButtons==1 && v.gamepadMoveWhileOpen && v.allInventory && v.timeMode==1 && v.slowPercent==30 &&
            v.language=="zh_CN" && v.theme=="frost" && v.hotkey==44 && v.actionModifier==6 &&
            v.wheelScale==1.25f && v.positionX==64 && v.positionY==32 && !v.sounds && !v.animations,
            "A fresh process reads all applied settings from disk");
        return 0;
    }
    const auto root=std::filesystem::path("build")/("settings-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto path=root/"FavoriteWheel.ini";
    const auto newPath=root/"Fresh.ini";
    SetSettingsPath(newPath.string());LoadSettings();
    Check(Config().language=="auto","Fresh installs follow the Windows display language");
    Check(!Config().allInventory,"Fresh installs show only favorites");
    Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Fresh installs keep equipment open and use LB/RB categories");
    Check(Config().timeMode==0 && Config().slowPercent==20,"Fresh installs retain pause and a 20% optional slowdown");
    Check(!Config().gamepadMoveWhileOpen,"Split sticks remain opt-in on fresh installs");
    BeginSettings();Check(SaveSettings(),"Save auto language");LoadSettings();
    Check(Config().language=="auto","Saving preserves automatic mode, not the resolved language");
    Check(Config().keepOpen,"Saving fresh defaults persists keep-open on");
    {std::ofstream file(path);file<<"; retained comment\n[General]\nChinese=0\n[Display]\nScalePercent=100\nDimPercent=95\nBlurStrength=100\nFont=C:/Windows/Fonts/msyh.ttc\n[Custom]\nKeep=123\n";}
    SetSettingsPath(path.string()); LoadSettings();
    Check(!Config().allInventory,"Old INIs retain favorites-only behavior");
    Check(!Config().gamepadMoveWhileOpen,"Old INIs keep left-stick wheel selection when the option is missing");
    Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Missing KeepOpen now defaults on without changing controller scheme");
    BeginSettings();auto continuous=Config();continuous.keepOpen=false;continuous.gamepadCategoryButtons=1;
    EditSettings(continuous);RevertSettings();Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Cancel restores continuous use and pad scheme");
    BeginSettings();EditSettings(continuous);Check(SaveSettings(),"Save continuous use and controller scheme");LoadSettings();
    Check(!Config().keepOpen && Config().gamepadCategoryButtons==1,"Explicit off and controller scheme round trip");
    BeginSettings();DefaultSettings();Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Defaults restore keep-open and LB/RB categories");RevertSettings();
    Check(Config().timeMode==0 && Config().slowPercent==20,"Old INIs retain pause without new time keys");
    BeginSettings();auto timeSettings=Config();timeSettings.timeMode=1;timeSettings.slowPercent=30;
    EditSettings(timeSettings);RevertSettings();Check(Config().timeMode==0,"Cancel restores time mode");
    BeginSettings();EditSettings(timeSettings);Check(SaveSettings(),"Save slow-time settings");LoadSettings();
    Check(Config().timeMode==1 && Config().slowPercent==30,"Time mode and factor survive reload");
    BeginSettings();DefaultSettings();Check(Config().timeMode==0 && Config().slowPercent==20,"Defaults restore pause and 20% factor");RevertSettings();
    Check(Config().timeMode==1 && Config().slowPercent==30,"Canceling defaults retains slow time");
    BeginSettings();auto inventorySettings=Config();inventorySettings.allInventory=true;
    EditSettings(inventorySettings);RevertSettings();Check(!Config().allInventory,"Cancelled item-source edit restores favorites");
    BeginSettings();EditSettings(inventorySettings);Check(SaveSettings(),"Save all-inventory source");LoadSettings();
    Check(Config().allInventory,"Item source survives reload");
    BeginSettings();DefaultSettings();Check(!Config().allInventory,"Defaults restore favorites-only source");RevertSettings();
    Check(Config().allInventory,"Cancelled defaults retain all-inventory source");
    Check(Config().language=="en","Legacy Chinese=0 migration");
    Check(Config().hotkeyModifier==0 && Config().actionHotkey==-1 && Config().actionModifier==1 &&
        Config().gamepadHotkey==-1 && Config().gamepadActionModifier==274,"Old INIs preserve Q / Shift+Q and controller defaults");
    BeginSettings();auto bindings=Config();bindings.hotkey=33;bindings.hotkeyModifier=2;
    bindings.actionHotkey=44;bindings.actionModifier=6;bindings.gamepadHotkey=273;bindings.gamepadModifier=280;bindings.gamepadActionModifier=275;
    EditSettings(bindings);Check(SaveSettings(),"Save separate keyboard/controller bindings");LoadSettings();
    Check(Config()==bindings,"Binding keys and modifiers round trip together");
    BeginSettings();bindings.hotkeyModifier=99;bindings.actionModifier=-4;bindings.gamepadHotkey=0;bindings.gamepadActionModifier=255;
    EditSettings(bindings);Check(Config().hotkeyModifier==7 && Config().actionModifier==0 && Config().gamepadHotkey==-1 && Config().gamepadActionModifier==274,"Invalid chord and controller values are bounded");
    RevertSettings();Check(Config().actionModifier==6,"Cancelled binding edit restores previous chords");
    Check(Config().overlayOpacity==35 && Config().positionY==46 && Config().wheelScale==1.f,"New defaults ignore obsolete dim and blur keys");
    Transition transition;
    Check(transition.Update(true,true,true,.11f)>.49f && transition.value<1.f,"Opening fade advances");
    Check(transition.Update(true,true,true,.11f)==1.f,"Opening completes in 220ms");
    Check(transition.Update(false,true,true,.05f)>.49f && transition.value<1.f,"Closing fade advances");
    Check(transition.Update(true,true,true,.014f)>.5f,"Rapid reopening reverses fade");
    Check(transition.Update(false,true,false,0)==0.f,"Forced close clears transition");
    Check(transition.Update(true,false,true,0)==1.f && transition.Update(false,false,true,0)==0.f,"Disabled animation switches immediately");
    transition.Update(true,true,true,1.f);
    Check(transition.Update(false,true,true,.22f)==0.f,"Closing completes in 220ms");
    transition.Update(true,true,true,.088f);
    const float pose=transition.value;
    transition.Update(false,true,true,.044f);
    transition.Update(true,true,true,.044f);
    Check(std::abs(transition.value-pose)<.0001f,"Reverse uses the same continuous pose");
    const float beforeNegative=transition.value;
    transition.Update(true,true,true,-1.f);
    Check(transition.value==beforeNegative,"Negative elapsed time does not reverse opening");
    Check(BladeExpansion(.5f,0,10)>.99f && BladeExpansion(.5f,9,10)<.01f,"Sweep separates almost completed and newly starting blades");
    Check(BladeExpansion(.75f,9,10)<1 && BladeExpansion(.75f,0,10)==1,"Closing folds the last blade before the first");
    for(int slot=0;slot<9;++slot) {
        Check(BladeExpansion(.4f,slot,10)>=BladeExpansion(.4f,slot+1,10),"Opening follows clockwise slot order");
        const float start=.48f*slot/9;
        Check(BladeExpansion(start,slot,10)==0 && BladeExpansion(start+.52f,slot,10)>.9999f,"Blade travel uses equal windows inside the total duration");
    }
    Check(std::abs(BladeExpansion(.26f,0,10)-.5f)<.0001f,"Blade travels evenly through its midpoint instead of front-loading motion");
    for(int slot=0;slot<10;++slot) {
        Check(BladeExpansion(0,slot,10)==0 && BladeExpansion(1,slot,10)==1,"All blades share total-duration endpoints");
        float previous=0;
        for(int frame=0;frame<=100;++frame) {
            const float progress=frame/100.f,blade=BladeExpansion(progress,slot,10);
            Check(std::isfinite(blade) && blade>=previous && blade<=1,"Blade geometry stays bounded and monotonic");
            Check(SmoothPhase(blade,.55f,1.f)>=0 && SmoothPhase(blade,.55f,1.f)<=1,"Content remains bounded");
            previous=blade;
        }
    }
    const auto original=Config();
    BeginSettings(); auto edited=Config(); edited.gamepadMoveWhileOpen=true; edited.switchKey=20;edited.wheelScale=1.25f; edited.positionX=64; edited.positionY=32; edited.overlayOpacity=50; edited.sounds=false; edited.animations=false; edited.theme="frost"; edited.hotkey=44; edited.language="zh_CN"; EditSettings(edited);
    Check(Config()==edited,"Live preview");
    RevertSettings(); Check(Config()==original,"Cancel restores every setting");
    BeginSettings(); EditSettings(edited); Check(SaveSettings(),"Save settings");
    RevertSettings(); Check(Config()==edited,"Closing after apply preserves applied values");
    LoadSettings(); Check(Config()==edited,"Settings survive reload");
    {
        // Persist and reload through an actual child process, not the same
        // Win32 profile cache. Use a Unicode path and change the working dir.
        const auto unicodePath=std::filesystem::absolute(root)/L"设置-持久化.ini";
        SetSettingsPath(utf8(unicodePath));Check(SaveSettings(),"Save to Unicode path");
        const auto working=std::filesystem::current_path();std::filesystem::current_path(root);
        LoadSettings();Check(Config()==edited,"Resolved path survives working directory changes");
        std::filesystem::current_path(working);
        std::wstring executable(32768,L'\0');executable.resize(GetModuleFileNameW(nullptr,executable.data(),32768));
        std::wstring command=L"\""+executable+L"\" --verify-persisted \""+unicodePath.wstring()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
        Check(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
            std::filesystem::absolute(root).c_str(),&startup,&process),"Launch independent settings reader");
        const auto wait=WaitForSingleObject(process.hProcess,10000);DWORD result=1;
        GetExitCodeProcess(process.hProcess,&result);CloseHandle(process.hThread);CloseHandle(process.hProcess);
        Check(wait==WAIT_OBJECT_0 && result==0,"Applied settings survive a fresh process");
        SetSettingsPath(path.string());
        // A read-only destination must not claim Apply succeeded or replace it.
        std::ifstream oldFile(path,std::ios::binary);
        const std::string before((std::istreambuf_iterator<char>(oldFile)),{});oldFile.close();
        Check(SetFileAttributesW(std::filesystem::absolute(path).c_str(),FILE_ATTRIBUTE_READONLY),"Set read-only fixture");
        BeginSettings();auto blocked=Config();blocked.positionX=12;EditSettings(blocked);
        const bool written=SaveSettings();const auto diagnostic=SettingsDiagnostic();
        SetFileAttributesW(std::filesystem::absolute(path).c_str(),FILE_ATTRIBUTE_NORMAL);
        std::ifstream unchangedFile(path,std::ios::binary);
        const std::string unchanged((std::istreambuf_iterator<char>(unchangedFile)),{});unchangedFile.close();
        Check(!written && diagnostic.find("open-target")!=std::string::npos && unchanged==before,"Read-only save failure is explicit and leaves original file intact");
        RevertSettings();LoadSettings();Check(Config()==edited,"Failed save does not change persisted settings");
        // Default game path is based on the executable, regardless of cwd.
        SetSettingsPath("");LoadSettings();const auto first=SettingsDiagnostic();
        std::filesystem::current_path(root);SetSettingsPath("");LoadSettings();
        Check(SettingsDiagnostic()==first,"Default path is independent of the launcher working directory");
        std::filesystem::current_path(working);SetSettingsPath(path.string());LoadSettings();
    }
    wchar_t value[32]{};
    GetPrivateProfileStringW(L"Custom",L"Keep",L"",value,32,std::filesystem::absolute(path).c_str());
    Check(std::wstring(value)==L"123","Unrelated INI keys survive");
    BeginSettings(); DefaultSettings(); RevertSettings(); Check(Config()==edited,"Defaults can be canceled");
    BeginSettings(); auto clamped=Config();clamped.wheelScale=99;clamped.positionX=-10;clamped.positionY=999;clamped.overlayOpacity=999;clamped.scale=99;clamped.sensitivity=-5;clamped.timeMode=99;clamped.slowPercent=-1; EditSettings(clamped);
    Check(Config().timeMode==2 && Config().slowPercent==5,"Time bounds enforced");
    Check(Config().scale==1.5f && Config().sensitivity==.2f,"Bounds enforced");
    Check(Config().wheelScale==1.5f && Config().positionX==0 && Config().positionY==100 && Config().overlayOpacity==80,"Layout and dimming bounds enforced");
    SetSettingsPath(root.string()); Check(!SaveSettings(),"Save failure reported");
    RevertSettings(); Check(Config()==edited,"Failed save remains cancelable");
    LoadResources("assets");
    Check(Languages().size()>=2 && Themes().size()>=2,"Bundled resources load");
    Check(Tr(edited,"settings")=="设置","UTF-8 language");
    Check(Tr(edited,"gamepadMoveWhileOpen")=="左摇杆控制移动" && Tr(edited,"padSettingsHelpRight").find("右摇杆")!=std::string::npos,"Split stick controls are localized");
    Check(Tr(edited,"spells")=="法术" && Tr(edited,"shouts")=="龙吼" && Tr(edited,"powers")=="能力","Separated magic categories use localized labels");
    Check(ResolveLanguage("auto","ZH-cn")=="zh_CN" && ResolveLanguage("zh-CN","en-US")=="zh_CN","Case/hyphen normalization and explicit override");
    Check(ResolveLanguage("auto","en-GB")=="en" && ResolveLanguage("auto","de-DE")=="en","English variants and unavailable locales fall back to English");
    Check(ResolveLanguage("auto","zh-TW")=="en","No unrelated regional translation is silently selected");
    Check(CycleLanguage("auto",1)=="en" && CycleLanguage("en",-1)=="auto" && CycleLanguage("zh-CN",1)=="auto","Automatic mode participates in language selection");
    edited.language="missing";Check(Tr(edited,"settings")=="SETTINGS","Unknown language fallback");
    const auto extra=root/"Resources"/"Languages";std::filesystem::create_directories(extra);
    {std::ofstream file(extra/"partial.ini");file<<"Name=Partial\nsettings=Custom\napply=\n";}
    {std::ofstream file(extra/"fr.ini");file<<"\xEF\xBB\xBFName=Fran\xC3\xA7" "ais\nFont=French.ttf\nsettings=Parametres\n";}
    {std::ofstream file(extra/"fr-CA.ini");file<<"Name=Canadian French\nsettings=Quebec\n";}
    {std::ofstream file(extra/"fr_CA.ini");file<<"Name=Duplicate\nsettings=Wrong duplicate\n";}
    {std::ofstream file(extra/"auto.ini");file<<"Name=Reserved\nsettings=Wrong\n";}
    {std::ofstream file(extra/"ja.ini",std::ios::binary);file<<"Name=Invalid UTF8\nsettings=";file.put(static_cast<char>(0xFF));}
    {std::ofstream file(extra/"de.ini",std::ios::binary);file<<std::string(256*1024+1,'x');}
    {std::ofstream file(extra/"es.ini",std::ios::binary);file<<"Name=Embedded NUL\nsettings=";file.put('\0');}
    const auto themeDir=root/"Resources"/"Themes";std::filesystem::create_directories(themeDir);
    {std::ofstream file(themeDir/"custom.ini");file<<"Accent=12AB34FF\nPanel=not-a-color\nBorderWidth=999\nOrnament=-2\nIconScale=nan\nTitleScale=1.1\nLabelScale=1x\nHoverDuration=inf\nPageDuration=0\n";}
    LoadResources((root/"Resources").string());
    Check(ResolveLanguage("auto","fr-CA")=="fr-CA" && ResolveLanguage("auto","fr-FR")=="fr","Exact locale takes priority over generic-language fallback");
    Check(CycleLanguage("fr-FR",-1)=="fr-CA","Language cycling starts from the displayed resolved choice");
    Check(ResolveLanguage("auto","ja-JP")=="en" && ResolveLanguage("auto","de-DE")=="en" && ResolveLanguage("auto","es-ES")=="en" && Languages().size()==4,"Malformed/oversized/reserved/duplicate catalogs are ignored");
    edited.language="fr_FR";
    Check(Tr(edited,"settings")=="Parametres" && FontPath(edited)=="French.ttf","Resolved translation and font use the same locale");
    edited.language="partial";Check(Tr(edited,"settings")=="Custom" && Tr(edited,"apply")=="APPLY" && Tr(edited,"cancel")=="CANCEL","Partial/empty translation fallback");
    Check(Tr(edited,"spells")=="SPELLS" && Tr(edited,"shouts")=="SHOUTS" && Tr(edited,"powers")=="POWERS","Older partial translations fall back for the new magic categories");
    edited.theme="custom";Check(Style(edited).accent==0xFF34AB12 && Style(edited).panel==Theme{}.panel,"RGBA color parsing and invalid color fallback");
    const auto& visual=Style(edited);
    Check(visual.borderWidth==2 && visual.ornament==0 && visual.iconScale==1 && visual.titleScale==1.1f && visual.labelScale==1 && visual.hoverDuration==.1f && visual.pageDuration==0,"Visual theme bounds, invalid/nonfinite fallback and zero-duration transitions");
    LoadResources((root/"missing").string());
    Check(Languages().size()==1 && Themes().size()==1 && Tr(edited,"apply")=="APPLY","Missing resource folder remains usable");
    edited.language="auto";
    Check(ActiveLanguage(edited)=="en" && LanguageLabel(edited)=="System (English)","Missing translations still show a usable automatic-language option");
    const auto automaticRoot=root/"Automatic";
    std::filesystem::create_directories(automaticRoot/"Languages");
    const auto detected=SystemLanguage();
    {std::ofstream file(automaticRoot/"Languages"/(detected+".ini"));file<<"Name=Detected UI\nFont=Detected.ttf\nsettings=Auto translated\n";}
    LoadResources(automaticRoot.string());
    Check(ActiveLanguage(edited)==detected && Tr(edited,"settings")=="Auto translated" && FontPath(edited)=="Detected.ttf","Automatic mode resolves real Windows UI locale for text and font together");
    Check(LanguageLabel(edited)=="System (Detected UI)","Automatic label includes the resolved language name");
    Check(Tr(edited,"apply")=="APPLY" && Tr(edited,"outfitTitle")=="OUTFIT PRESETS" && Tr(edited,"lightWrongThread").starts_with("Face Lighting rejected") &&
        Tr(edited,"quickSlotHint")=="HOVER + 1-8 BIND / UNBIND QUICK SLOT","Incomplete automatic catalog has complete English fallbacks");
    Check(WheelSlot(2,0)==-1 && WheelSlot(0,-1)==0 && WheelSlot(0,0)==-1,"Free pointer cannot use outside wheel or in center");
    Check(!applyButton.Contains(cancelButton.x+10,cancelButton.y+10),"Apply and cancel do not overlap");
    for(int tab:{0,1,2,3})for(int slot=0;slot<SettingCount(tab);++slot) {
        const auto valueRect=ValueButton(slot);
        Check(valueRect.Contains(valueRect.x+5,valueRect.y+5) && !MinusButton(slot).Contains(valueRect.x+5,valueRect.y+5),"Setting value and decrement targets are distinct");
        Check(!generalTab.Contains(valueRect.x+5,valueRect.y+5) && !controlsTab.Contains(valueRect.x+5,valueRect.y+5) && !gamepadTab.Contains(valueRect.x+5,valueRect.y+5) && !gameplayTab.Contains(valueRect.x+5,valueRect.y+5) && valueRect.y+valueRect.h<145,"Settings rows stay below tabs and above help text");
    }
    Check(!generalTab.Contains(controlsTab.x+5,controlsTab.y+5) && !controlsTab.Contains(gameplayTab.x+5,gameplayTab.y+5),"Three settings tabs have distinct targets");
    Check(!controlsTab.Contains(gamepadTab.x+5,gamepadTab.y+5) && !gamepadTab.Contains(gameplayTab.x+5,gameplayTab.y+5),"Controller tab has a separate hit region");
    Check(SettingCount(2)==4 && SettingRow(2,0)==18 && SettingRow(2,1)==19 && SettingRow(2,2)==20 && SettingRow(2,3)==22,"Gameplay tab includes continuous use");
    Check(SettingCount(1)==5 && SettingCount(3)==5 && SettingRow(3,3)==21 && SettingRow(3,4)==23,"Controller tab includes separate category and split-stick settings");
    Check(NextSettingsTab(1,1)==3 && NextSettingsTab(3,1)==2 && NextSettingsTab(2,1)==0 && NextSettingsTab(0,-1)==2,"Controller cycles all four tabs in visual order");
    std::cout<<"Settings persistence, cancellation, failure, localization, themes and hit regions passed\n";
}

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
void Ac1_Utf8ThemeId();
void Ac2_UnreadableThemeEntry();
void Ac5_BuiltinEnglishCoverage();
void Ac8_TranslationCacheConsistency();
int wmain(int argc,wchar_t** argv) {
    using namespace Wheel;
    auto utf8=[](const std::filesystem::path& path){const auto text=path.u8string();return std::string(reinterpret_cast<const char*>(text.data()),text.size());};
    if(argc==3 && std::wstring_view(argv[1])==L"--verify-persisted") {
        SetSettingsPath(utf8(argv[2]));LoadSettings();const auto v=Config();
        Check(!v.keepOpen && v.gamepadCategoryButtons==1 && v.gamepadMoveWhileOpen && v.swapMouseHands && !v.hideEmptyCategories && v.allInventory && v.timeMode==1 && v.slowPercent==70 &&
            v.language=="zh_CN" && v.theme=="frost" && v.hotkey==44 && v.actionModifier==6 &&
            v.wheelScale==1.25f && v.positionX==64 && v.positionY==32 && !v.sounds && !v.animations,
            "A fresh process reads all applied settings from disk");
        return 0;
    }
    const auto root=std::filesystem::path("build")/("settings-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    const auto path=root/"FavoriteWheel.ini";
    auto bytes=[](const std::filesystem::path& file) {
        std::ifstream input(file,std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(input)),{});
    };
    // No installer INI: initialization stays read-only, then Apply creates a
    // complete document even with Unicode paths and absent parent directories.
    const auto firstRun=root/L"首次配置"/L"缺少目录"/L"FavoriteWheel.ini";
    SetSettingsPath(utf8(firstRun));LoadSettings();
    Check(Config()==Settings{},"Missing INI initializes every field from struct defaults");
    Check(!std::filesystem::exists(firstRun.parent_path()) && SettingsDiagnostic().find("load-defaults")!=std::string::npos,
        "Startup does not create files or directories when the main INI is absent");
    BeginSettings();auto firstSettings=Config();firstSettings.enabled=false;firstSettings.font=utf8(L"D:/字体/自定义.ttf");EditSettings(firstSettings);
    Check(SaveSettings(),"First Apply creates a complete INI including Enabled and Unicode Font");
    LoadSettings();Check(Config()==firstSettings,"First-created INI round trips every field");
    std::filesystem::remove(firstRun);LoadSettings();
    Check(Config()==Settings{} && !std::filesystem::exists(firstRun),"Deleting INI resets memory defaults without recreating it at startup");
    const auto empty=root/"Empty.ini";
    {std::ofstream file(empty,std::ios::binary);}
    SetSettingsPath(empty.string());LoadSettings();
    Check(Config()==Settings{} && std::filesystem::file_size(empty)==0,"Existing empty INI uses defaults and is never rewritten at startup");
    Check(SettingsDiagnostic().find("loaded-empty")!=std::string::npos && SettingsDiagnostic().find("bytes=0")!=std::string::npos,
        "Empty configuration is distinguished from a successful nonempty load");
    BeginSettings();auto recovered=Config();recovered.font=utf8(L"D:/字体/𠀀.ttf");recovered.positionX=64;EditSettings(recovered);
    Check(SaveSettings(),"Apply repairs an empty INI even with characters outside ANSI code pages");
    const auto repairedBytes=bytes(empty);
    Check(repairedBytes.size()>2 && static_cast<unsigned char>(repairedBytes[0])==0xFF && static_cast<unsigned char>(repairedBytes[1])==0xFE,
        "Repaired empty INI uses Unicode encoding");
    LoadSettings();Check(Config()==recovered,"Repaired empty INI round trips Unicode settings");
    const auto blockedParent=root/"NotADirectory";
    {std::ofstream file(blockedParent);file<<"preserve";}
    SetSettingsPath(utf8(blockedParent/"FavoriteWheel.ini"));LoadSettings();BeginSettings();
    Check(!SaveSettings() && SettingsDiagnostic().find("create-directory")!=std::string::npos && bytes(blockedParent)=="preserve",
        "Blocked first-run directory creation fails explicitly without replacing the obstruction");
    RevertSettings();
    SetSettingsPath(root.string());LoadSettings();
    Check(Config()==Settings{} && SettingsDiagnostic().find("load-unavailable")!=std::string::npos,
        "A directory at the INI path uses safe defaults and is not reported as a loaded file");
    const auto lowTimePath=root/"Legacy20.ini";
    {std::ofstream file(lowTimePath);file<<"; player-owned legacy config\n[General]\nTimeMode=1\nSlowTimePercent=20\n[Custom]\nKeep=123\n";}
    const auto lowTimeBytes=bytes(lowTimePath);
    SetSettingsPath(lowTimePath.string());LoadSettings();
    Check(Config().timeMode==1 && Config().slowPercent==50 && bytes(lowTimePath)==lowTimeBytes,
        "Legacy 20% uses 50% in memory without rewriting the player-owned INI at startup");
    BeginSettings();auto lowTimeEdit=Config();lowTimeEdit.slowPercent=5;EditSettings(lowTimeEdit);
    Check(Config().slowPercent==50,"Editing cannot bypass the physics floor");RevertSettings();
    Check(bytes(lowTimePath)==lowTimeBytes,"Cancel leaves legacy low-speed settings untouched");
    BeginSettings();Check(SaveSettings(),"Apply explicitly persists the clamped speed");LoadSettings();
    Check(Config().slowPercent==50 && bytes(lowTimePath).find("SlowTimePercent=50")!=std::string::npos,
        "Applied effective speed survives a reload");
    const auto newPath=root/"Fresh.ini";
    SetSettingsPath(newPath.string());LoadSettings();
    Check(Config().language=="auto","Fresh installs follow the Windows display language");
    Check(!Config().allInventory,"Fresh installs show only favorites");
    Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Fresh installs keep equipment open and use LB/RB categories");
    Check(Config().timeMode==0 && Config().slowPercent==50,"Fresh installs retain pause and a 50% optional slowdown");
    Check(!Config().gamepadMoveWhileOpen,"Split sticks remain opt-in on fresh installs");
    Check(!Config().rememberLastWheel,"Wheel memory remains opt-in for existing and fresh configurations");
    BeginSettings();auto remembered=Config();remembered.rememberLastWheel=true;EditSettings(remembered);RevertSettings();
    Check(!Config().rememberLastWheel,"Cancel does not enable wheel memory");
    BeginSettings();EditSettings(remembered);Check(SaveSettings(),"Save wheel memory option");LoadSettings();
    Check(Config().rememberLastWheel,"Wheel memory option persists across configuration reloads");
    Check(Config().positionX==72 && Config().hideEmptyCategories,"Fresh installs use right-side placement and hide empty categories");
    BeginSettings();Check(SaveSettings(),"Save auto language");LoadSettings();
    Check(Config().language=="auto","Saving preserves automatic mode, not the resolved language");
    Check(Config().keepOpen,"Saving fresh defaults persists keep-open on");
    {std::ofstream file(path);file<<"; retained comment\n[General]\nChinese=0\n[Display]\nScalePercent=100\nDimPercent=95\nBlurStrength=100\nFont=C:/Windows/Fonts/msyh.ttc\n[Custom]\nKeep=123\n";}
    const auto initialBytes=bytes(path);
    SetSettingsPath(path.string()); LoadSettings();
    Check(bytes(path)==initialBytes,"Startup leaves partial legacy settings and comments byte-for-byte intact");
    Check(!Config().allInventory,"Old INIs retain favorites-only behavior");
    Check(!Config().swapMouseHands,"Old INIs retain LMB right hand without rewriting them");
    Check(!Config().gamepadMoveWhileOpen,"Old INIs keep left-stick wheel selection when the option is missing");
    Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Missing KeepOpen now defaults on without changing controller scheme");
    BeginSettings();auto continuous=Config();continuous.keepOpen=false;continuous.gamepadCategoryButtons=1;
    EditSettings(continuous);RevertSettings();Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Cancel restores continuous use and pad scheme");
    BeginSettings();EditSettings(continuous);Check(SaveSettings(),"Save continuous use and controller scheme");LoadSettings();
    Check(!Config().keepOpen && Config().gamepadCategoryButtons==1,"Explicit off and controller scheme round trip");
    BeginSettings();DefaultSettings();Check(Config().keepOpen && Config().gamepadCategoryButtons==0,"Defaults restore keep-open and LB/RB categories");RevertSettings();
    Check(Config().timeMode==0 && Config().slowPercent==50,"Old INIs retain pause without new time keys");
    BeginSettings();auto timeSettings=Config();timeSettings.timeMode=1;timeSettings.slowPercent=70;
    EditSettings(timeSettings);RevertSettings();Check(Config().timeMode==0,"Cancel restores time mode");
    BeginSettings();EditSettings(timeSettings);Check(SaveSettings(),"Save slow-time settings");LoadSettings();
    Check(Config().timeMode==1 && Config().slowPercent==70,"Time mode and factor survive reload");
    BeginSettings();DefaultSettings();Check(Config().timeMode==0 && Config().slowPercent==50,"Defaults restore pause and 50% factor");RevertSettings();
    Check(Config().timeMode==1 && Config().slowPercent==70,"Canceling defaults retains slow time");
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
    BeginSettings(); auto edited=Config(); edited.swapMouseHands=true; edited.gamepadMoveWhileOpen=true;edited.hideEmptyCategories=false; edited.switchKey=20;edited.wheelScale=1.25f; edited.positionX=64; edited.positionY=32; edited.overlayOpacity=50; edited.sounds=false; edited.animations=false; edited.theme="frost"; edited.hotkey=44; edited.language="zh_CN"; EditSettings(edited);
    Check(Config()==edited,"Live preview");
    Check(!AppliedGamepadMoveWhileOpen(),"Pending toggle does not change the active selection stick");
    RevertSettings(); Check(Config()==original,"Cancel restores every setting");
    Check(!AppliedGamepadMoveWhileOpen(),"Cancel leaves the active stick scheme unchanged");
    BeginSettings(); EditSettings(edited); Check(SaveSettings(),"Save settings");
    Check(AppliedGamepadMoveWhileOpen(),"Successful Apply activates the new stick scheme");
    RevertSettings(); Check(Config()==edited,"Closing after apply preserves applied values");
    LoadSettings(); Check(Config()==edited,"Settings survive reload");
    BeginSettings();auto pendingOff=Config();pendingOff.gamepadMoveWhileOpen=false;EditSettings(pendingOff);
    Check(AppliedGamepadMoveWhileOpen(),"Pending disable preserves the right stick until Apply");
    DefaultSettings();Check(AppliedGamepadMoveWhileOpen() && !Config().gamepadMoveWhileOpen && Config().positionX==72,
        "Defaults preview preserves the applied stick scheme and restores right-side placement");
    RevertSettings();Check(AppliedGamepadMoveWhileOpen(),"Canceling defaults preserves the applied right-stick scheme");
    BeginSettings();EditSettings(pendingOff);Check(SaveSettings(),"Apply left-stick selection again");
    Check(!AppliedGamepadMoveWhileOpen(),"Successful disable restores left-stick selection");
    BeginSettings();EditSettings(edited);Check(SaveSettings(),"Restore applied right-stick selection");
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
        LoadSettings();Check(Config()==edited && bytes(path)==before,"Read-only settings load without modification");
        BeginSettings();auto blocked=Config();blocked.positionX=12;blocked.gamepadMoveWhileOpen=false;EditSettings(blocked);
        const bool written=SaveSettings();const auto diagnostic=SettingsDiagnostic();
        SetFileAttributesW(std::filesystem::absolute(path).c_str(),FILE_ATTRIBUTE_NORMAL);
        std::ifstream unchangedFile(path,std::ios::binary);
        const std::string unchanged((std::istreambuf_iterator<char>(unchangedFile)),{});unchangedFile.close();
        Check(!written && diagnostic.find("open-target")!=std::string::npos && unchanged==before,"Read-only save failure is explicit and leaves original file intact");
        Check(AppliedGamepadMoveWhileOpen(),"Failed Apply cannot switch the active stick scheme");
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
    Check(Config().timeMode==2 && Config().slowPercent==50,"Time bounds enforced");
    Check(Config().scale==1.5f && Config().sensitivity==.2f,"Bounds enforced");
    Check(Config().wheelScale==1.5f && Config().positionX==0 && Config().positionY==100 && Config().overlayOpacity==80,"Layout and dimming bounds enforced");
    SetSettingsPath(root.string()); Check(!SaveSettings(),"Save failure reported");
    RevertSettings(); Check(Config()==edited,"Failed save remains cancelable");
    LoadResources("assets");
    Check(Languages().size()==8 && Themes().size()>=2,"All eight bundled languages load");
    auto skin=Config();skin.theme="skyrim";
    Check(Themes().size()==3 && Style(skin).style==ThemeStyle::Skyrim,"Only the three current bundled themes load");
    Check(Style(skin).frameStyle==FrameStyle::Nordic && Style(skin).surfaceTexture.ends_with("Textures/skyrim-charcoal.png") &&
        Style(skin).materialStrength==.9f,"Skyrim skin enables its external material and Nordic framing");
    skin.theme="classic";
    Check(Style(skin).style==ThemeStyle::ClassicGold && Style(skin).name=="Classic Gold" &&
        Style(skin).accent==Theme{}.accent && Style(skin).panel==Theme{}.panel && Style(skin).surfaceTexture.empty(),
        "Classic Gold uses the redesigned components and the built-in fallback palette");
    for(const auto& theme:Themes())Check(theme.id=="classic" || theme.id=="skyrim" || theme.id=="polygon","Retired themes are absent from the bundled selector");
    skin.theme="polygon";Check(Style(skin).style==ThemeStyle::Skyrim && Style(skin).layout.shape==WheelShape::Polygon,
        "Skyrim Polygon retains the Nordic renderer and ten-sided shape");
    for(const auto& id:{"frost","engraved","minimal","ellipse","missing-theme"}) {
        skin.theme=id;Check(Style(skin).id=="classic" && Style(skin).style==ThemeStyle::ClassicGold,
            "Unavailable retired theme settings display the new Classic Gold fallback");
    }
    for(const auto& language:Languages())for(const auto& [key,value]:Languages().front().text) {
        Check(language.text.contains(key),"Every bundled catalog contains the current English keys");
        Check(key=="Font" || !language.text.at(key).empty(),"Bundled translations do not silently fall back through empty values");
    }
    for(const auto& [locale,id]:{std::pair{"de-DE","de"},std::pair{"ru-RU","ru"},std::pair{"ja-JP","ja"},std::pair{"ko-KR","ko"},std::pair{"pt-BR","pt_BR"}})
        Check(ResolveLanguage("auto",locale)==id,"New bundled locales resolve automatically");
    Check(ResolveLanguage("auto","pt-PT")=="en","Brazilian Portuguese is not substituted for Portugal");
    auto japanese=edited;japanese.language="ja";
    Check(FontPath(japanese)!="auto" && !FallbackFontPaths().empty(),"Automatic installed font selection resolves before rendering");
    Check(ResolveLanguage("auto","fr-FR")=="fr" && ResolveLanguage("auto","fr-CA")=="fr",
        "Bundled generic French resolves regional Windows locales");
    auto french=edited;french.language="fr";
    Check(Tr(french,"settings")=="PARAMÈTRES" && Tr(french,"hideEmptyCategories")=="Masquer les catégories vides" &&
        Tr(french,"outfitExported").find("FavoriteWheel/Outfits/Exports")!=std::string::npos,
        "Community French and current settings load with the real export path");
    Check(Tr(edited,"settings")=="设置","UTF-8 language");
    Check(Tr(edited,"gamepadMoveWhileOpen")=="左摇杆控制移动" && Tr(edited,"padSettingsHelpRight").find("右摇杆")!=std::string::npos,"Split stick controls are localized");
    Check(Tr(edited,"spells")=="法术" && Tr(edited,"shouts")=="龙吼" && Tr(edited,"powers")=="能力","Separated magic categories use localized labels");
    Check(ResolveLanguage("auto","ZH-cn")=="zh_CN" && ResolveLanguage("zh-CN","en-US")=="zh_CN","Case/hyphen normalization and explicit override");
    Check(ResolveLanguage("auto","en-GB")=="en" && ResolveLanguage("auto","es-ES")=="en","English variants and unavailable locales fall back to English");
    Check(ResolveLanguage("auto","zh-TW")=="en","No unrelated regional translation is silently selected");
    Check(CycleLanguage("auto",1)=="en" && CycleLanguage("en",-1)=="auto" && CycleLanguage(Languages().back().id,1)=="auto","Automatic mode participates in language selection");
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
    {std::ofstream file(themeDir/"custom.ini");file<<"Style=unknown\nAccent=12AB34FF\nPanel=not-a-color\nBorderWidth=999\nOrnament=-2\nIconScale=nan\nTitleScale=1.1\nLabelScale=1x\nHoverDuration=inf\nPageDuration=0\n";}
    {std::ofstream file(themeDir/"skin.ini");file<<"Style= SkYrIm \nName=Custom Nordic\nAccent=AABBCCFF\nFrameStyle= NoRdIc \nSurfaceTexture=Textures/custom.png\nMaterialStrength=99\nMaterialZoom=0\n";}
    for(const auto& [id,texture]:{std::pair{"escape","../outside.png"},std::pair{"absolute","C:/outside.png"},
        std::pair{"rooted","/outside.png"},std::pair{"wrong-format","Textures/file.dds"}})
        {std::ofstream file(themeDir/(std::string(id)+".ini"));file<<"SurfaceTexture="<<texture<<"\nFrameStyle=unknown\nMaterialStrength=nan\nMaterialZoom=inf\n";}
    std::ofstream(root/"Resources/Themes/layout-ellipse.ini")<<"WheelShape=  ELLIPSE  \nWheelStretchX=99\nWheelStretchY=0.1\nDetailPosition= LeFt \nDetailGap=-1\n";
    std::ofstream(root/"Resources/Themes/layout-polygon.ini")<<"WheelShape=polygon\nDetailPosition=center\nWheelStretchX=nan\nDetailGap=inf\n";
    std::ofstream(root/"Resources/Themes/layout-unknown.ini")<<"WheelShape=unknown\nDetailPosition=invalid\nWheelStretchX=invalid\nWheelStretchY=1.1junk\n";
    LoadResources((root/"Resources").string());
    Check(ResolveLanguage("auto","fr-CA")=="fr-CA" && ResolveLanguage("auto","fr-FR")=="fr","Exact locale takes priority over generic-language fallback");
    Check(CycleLanguage("fr-FR",-1)=="fr-CA","Language cycling starts from the displayed resolved choice");
    Check(ResolveLanguage("auto","ja-JP")=="en" && ResolveLanguage("auto","de-DE")=="en" && ResolveLanguage("auto","es-ES")=="en" && Languages().size()==4,"Malformed/oversized/reserved/duplicate catalogs are ignored");
    edited.language="fr_FR";
    Check(Tr(edited,"settings")=="Parametres" && FontPath(edited)=="French.ttf","Resolved translation and font use the same locale");
    edited.language="partial";Check(Tr(edited,"settings")=="Custom" && Tr(edited,"apply")=="APPLY" && Tr(edited,"cancel")=="CANCEL","Partial/empty translation fallback");
    Check(Tr(edited,"spells")=="SPELLS" && Tr(edited,"shouts")=="SHOUTS" && Tr(edited,"powers")=="POWERS","Older partial translations fall back for the new magic categories");
    edited.theme="custom";Check(Style(edited).accent==0xFF34AB12 && Style(edited).panel==Theme{}.panel,"RGBA color parsing and invalid color fallback");
    Check(Style(edited).style==ThemeStyle::Etched,"Unknown drawing styles fall back without disabling custom theme colors");
    const auto& visual=Style(edited);
    Check(visual.borderWidth==2 && visual.ornament==0 && visual.iconScale==1 && visual.titleScale==1.1f && visual.labelScale==1 && visual.hoverDuration==.1f && visual.pageDuration==0,"Visual theme bounds, invalid/nonfinite fallback and zero-duration transitions");
    Check(visual.layout.shape==WheelShape::Circle && visual.layout.detailPosition==DetailPosition::Auto,"Legacy themes keep their circular layout and automatic card placement");
    edited.theme="layout-ellipse";
    Check(Style(edited).layout.shape==WheelShape::Ellipse && Style(edited).layout.stretchX==1.25f && Style(edited).layout.stretchY==.9f &&
        Style(edited).layout.detailPosition==DetailPosition::Left && Style(edited).layout.detailGap==16,"Layout tokens accept case/whitespace and numeric bounds");
    edited.theme="layout-polygon";
    Check(Style(edited).layout.shape==WheelShape::Polygon && Style(edited).layout.detailPosition==DetailPosition::Center &&
        Style(edited).layout.stretchX==1.2f && Style(edited).layout.detailGap==20,"Polygon/centre tokens load and nonfinite layout values fall back");
    edited.theme="layout-unknown";
    Check(Style(edited).layout.shape==WheelShape::Circle && Style(edited).layout.detailPosition==DetailPosition::Auto &&
        Style(edited).layout.stretchX==1.2f && Style(edited).layout.stretchY==.94f,"Unknown layout tokens and malformed numbers keep safe defaults");
    edited.theme="skin";Check(Style(edited).style==ThemeStyle::Skyrim && Style(edited).accent==0xFFCCBBAA && Style(edited).name=="Custom Nordic",
        "Custom skin Style accepts whitespace/case and keeps independent color/name customization");
    Check(Style(edited).frameStyle==FrameStyle::Nordic && Style(edited).surfaceTexture.ends_with("Textures/custom.png") &&
        Style(edited).materialStrength==1 && Style(edited).materialZoom==1,"Skin material bounds and relative resource resolution");
    for(const auto id:{"escape","absolute","rooted","wrong-format"}) {
        edited.theme=id;const auto& rejected=Style(edited);
        Check(rejected.surfaceTexture.empty() && rejected.frameStyle==FrameStyle::Plain && rejected.materialStrength==.85f && rejected.materialZoom==1,
            "Escaping/absolute/unsupported texture paths and invalid material numbers safely fall back");
    }
    LoadResources((root/"missing").string());
    Check(Languages().size()==1 && Themes().size()==1 && Tr(edited,"apply")=="APPLY","Missing resource folder remains usable");
    Check(Style(edited).style==ThemeStyle::ClassicGold && Style(edited).name=="Classic Gold","Missing theme files keep the redesigned default usable");
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
    Check(SettingCount(2)==6 && SettingRow(2,0)==18 && SettingRow(2,1)==19 && SettingRow(2,2)==20 && SettingRow(2,3)==22 && SettingRow(2,4)==24 && SettingRow(2,5)==26,"Gameplay tab includes continuous use, empty categories and wheel memory");
    Check(SettingCount(1)==6 && SettingRow(1,5)==25 && SettingCount(3)==5 && SettingRow(3,3)==21 && SettingRow(3,4)==23,"Keyboard tab includes mouse hands; controller retains its separate settings");
    Check(NextSettingsTab(1,1)==3 && NextSettingsTab(3,1)==2 && NextSettingsTab(2,1)==0 && NextSettingsTab(0,-1)==2,"Controller cycles all four tabs in visual order");
    Ac1_Utf8ThemeId();Ac2_UnreadableThemeEntry();Ac5_BuiltinEnglishCoverage();Ac8_TranslationCacheConsistency();
    std::cout<<"Settings persistence, cancellation, failure, localization, themes and hit regions passed\n";
}
// ---------------------------------------------------------------------------
// Acceptance tests for task 20261028-takeover-review (AC-1, AC-2, AC-5, AC-8).
// Each function is self-contained so T-1, T-4 and T-6 can be merged separately.
// ---------------------------------------------------------------------------
namespace {
    std::string Utf8(const std::filesystem::path& path) {
        const auto text=path.u8string();return std::string(reinterpret_cast<const char*>(text.data()),text.size());
    }
    std::filesystem::path Scratch(const char* name) {
        const auto root=std::filesystem::path("build")/("acceptance-"+std::string(name)+"-"+std::to_string(GetCurrentProcessId()));
        std::error_code error;std::filesystem::remove_all(root,error);
        std::filesystem::create_directories(root);return root;
    }
    void RemoveScratch(const std::filesystem::path& root) {
        std::error_code error;(void)std::filesystem::remove_all(root,error);
    }
    bool TryLoad(const std::string& root,const char* message) {
        try {Wheel::LoadResources(root);return true;}
        catch(const std::exception& error) {std::cerr<<message<<": "<<error.what()<<'\n';return false;}
        catch(...) {std::cerr<<message<<": unknown exception\n";return false;}
    }
    std::vector<std::string> CatalogKeys() {
        std::vector<std::string> keys;
        std::ifstream catalog("assets/Languages/en.ini",std::ios::binary);std::string line;
        while(std::getline(catalog,line)) {
            if(!line.empty() && line.back()=='\r')line.pop_back();
            const auto equal=line.find('=');
            if(equal!=std::string::npos && equal)keys.push_back(line.substr(0,equal));
        }
        return keys;
    }
}
// AC-1: a theme file named in UTF-8 must produce a UTF-8 theme ID that survives
// Apply and the next LoadSettings.
void Ac1_Utf8ThemeId() {
    using namespace Wheel;
    const auto root=Scratch("ac1"),resources=root/"Resources",themes=resources/"Themes";
    std::filesystem::create_directories(themes);
    const std::u8string name=u8"测试主题";
    {std::ofstream file(themes/std::filesystem::path(name+u8".ini"),std::ios::binary);
     file<<"Name=测\nAccent=12AB34FF\nPanel=010203FF\nBorder=44556677\n";}
    Check(TryLoad(resources.string(),"AC-1: LoadResources must accept a UTF-8 theme file name"),
        "AC-1: a UTF-8 theme file loads");
    const std::string wanted(name.begin(),name.end());
    const Theme* selected=nullptr;
    for(const auto& theme:Themes())if(theme.id==wanted)selected=&theme;
    Check(selected!=nullptr,"AC-1: Themes() exposes the UTF-8 theme ID taken from the file name");
    Check(selected->name=="测","AC-1: the UTF-8 theme Name is read");
    Check(selected->accent==0xFF34AB12 && selected->panel==0xFF030201 && selected->border==0x77665544,
        "AC-1: the custom theme color keys still parse for a UTF-8 theme");
    SetSettingsPath(Utf8(root/"Settings.ini"));LoadSettings();
    BeginSettings();auto edited=Config();edited.theme=wanted;EditSettings(edited);
    Check(SaveSettings(),"AC-1: a UTF-8 theme ID can be applied and saved");
    LoadSettings();
    Check(Config().theme==wanted,"AC-1: the saved UTF-8 theme ID survives a reload");
    Check(Style(Config()).id==wanted,"AC-1: the reloaded UTF-8 theme is the active style");
    RemoveScratch(root);
}
// AC-2: damaged or unreadable entries in the resource folders must not abort
// resource loading; the built-in English catalog and the classic theme stay.
void Ac2_UnreadableThemeEntry() {
    using namespace Wheel;
    const auto root=Scratch("ac2"),resources=root/"Resources",themes=resources/"Themes",languages=resources/"Languages";
    std::filesystem::create_directories(themes);std::filesystem::create_directories(languages);
    {std::ofstream file(themes/"plain.ini",std::ios::binary);file<<"Name=Plain\nAccent=FFFFFFFF\n";}
    std::filesystem::create_directories(themes/"folder.ini"); // A directory cannot be a theme.
    {std::ofstream file(themes/"empty.ini",std::ios::binary);}
    {std::ofstream file(themes/"broken.ini",std::ios::binary);file.put(static_cast<char>(0xFF));}
    {std::ofstream file(languages/"broken.ini",std::ios::binary);file.put(static_cast<char>(0xFF));}
    const auto busy=themes/"busy.ini";
    {std::ofstream file(busy,std::ios::binary);file<<"Name=Busy\nAccent=11223344\n";}
    const HANDLE guard=CreateFileW(busy.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    Check(guard!=INVALID_HANDLE_VALUE,"AC-2: the fixture locks one theme entry against every reader");
    const bool loaded=TryLoad(resources.string(),"AC-2: LoadResources must not throw on an unreadable theme entry");
    CloseHandle(guard);
    bool classic=false;for(const auto& theme:Themes())if(theme.id=="classic")classic=true;
    bool english=false;for(const auto& language:Languages())if(language.id=="en")english=true;
    Settings config;config.language="en";config.theme="classic";
    Check(loaded,"AC-2: unreadable, empty and unparsable entries are skipped instead of aborting loading");
    Check(classic && Style(config).style==ThemeStyle::ClassicGold,"AC-2: the built-in classic theme stays usable");
    Check(english && Tr(config,"apply")=="APPLY","AC-2: the built-in English catalog stays usable");
    const auto fileRoot=root/"not-a-directory";
    {std::ofstream file(fileRoot,std::ios::binary);file<<"plain file";}
    Check(TryLoad(Utf8(fileRoot),"AC-2: a file at the resource root must not throw"),
        "AC-2: a resource root that is a file still leaves the built-in catalogs");
    RemoveScratch(root);
}
// AC-5: every key of the bundled English catalog except Name/Font needs a built-in
// English value, and the UI glyph string must preheat the swap-mouse-hands text.
void Ac5_BuiltinEnglishCoverage() {
    using namespace Wheel;
    const auto keys=CatalogKeys();
    Check(keys.size()>300,"AC-5: the bundled English catalog lists the user-visible keys");
    const auto root=Scratch("ac5");
    Settings config;config.language="en";
    Check(TryLoad(root.string(),"AC-5: an empty resource root falls back to the built-in catalogs"),
        "AC-5: the built-in catalog loads without any language files");
    std::size_t checked=0;
    for(const auto& key:keys) {
        if(key=="Name" || key=="Font")continue;
        const auto message=std::string("AC-5: the built-in English table has no value for ")+key;
        Check(Tr(config,key)!=key,message.c_str());++checked;
    }
    Check(checked>=320,"AC-5: every catalog key except Name/Font was checked");
    const auto swapped=Tr(config,"swapMouseHands");
    Check(swapped!="swapMouseHands" && UIGlyphs(config).find(swapped)!=std::string::npos,
        "AC-5: the UI glyph string preheats the swapMouseHands translation");
    RemoveScratch(root);
}
// AC-8: the translation and glyph caches must answer exactly like a fresh
// LoadResources, for all eight bundled languages, and must follow language and
// hotkey changes (plan §8).
void Ac8_TranslationCacheConsistency() {
    using namespace Wheel;
    const auto keys=CatalogKeys();
    Check(keys.size()>300,"AC-8: the bundled English catalog lists the translation keys");
    Check(TryLoad("assets","AC-8: the bundled resources load"),"AC-8: the bundled resources load");
    Check(Languages().size()==8,"AC-8: all eight bundled languages are loaded");
    std::vector<std::string> ids;
    for(const auto& language:Languages())ids.push_back(language.id);
    std::vector<std::vector<std::string>> reference(ids.size(),std::vector<std::string>(keys.size()));
    for(std::size_t language=0;language<ids.size();++language) {
        Check(TryLoad("assets","AC-8: the bundled resources reload"),"AC-8: the bundled resources reload");
        Settings config;config.language=ids[language];
        for(std::size_t key=0;key<keys.size();++key)reference[language][key]=Tr(config,keys[key]);
    }
    Check(TryLoad("assets","AC-8: the bundled resources reload"),"AC-8: the bundled resources reload");
    for(std::size_t key=0;key<keys.size();++key)for(std::size_t language=0;language<ids.size();++language) {
        Settings config;config.language=ids[language];
        const auto cached=Tr(config,keys[key]);
        const auto message=std::string("AC-8: cached Tr differs from a fresh load for ")+ids[language]+"/"+keys[key];
        Check(cached==reference[language][key],message.c_str());
    }
    Settings english;english.language="en";
    Settings chinese;chinese.language="zh_CN";
    const auto englishGlyphs=UIGlyphs(english),chineseGlyphs=UIGlyphs(chinese);
    Check(englishGlyphs!=chineseGlyphs,"AC-8: switching the language changes the UI glyph string");
    Check(UIGlyphs(english)==englishGlyphs,"AC-8: switching back restores the original UI glyph string");
    Settings rebound=english;rebound.switchKey=20;
    Check(UIGlyphs(rebound)!=englishGlyphs,"AC-8: rebinding the switch key refreshes the UI glyph string");
    Check(UIGlyphs(english)==englishGlyphs,"AC-8: restoring the switch key restores the UI glyph string");
}

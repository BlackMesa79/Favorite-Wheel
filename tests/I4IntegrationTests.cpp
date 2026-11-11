#include "I4Textures.h"
#include "I4Entry.h"
#include "I4Diagnostics.h"
#include "I4LoadLog.h"
#include "IconLibrary.h"
#include "SwfRasterFixtures.h"
#include <d3d11.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <deque>
#include <future>

namespace {
using namespace Wheel;
using Microsoft::WRL::ComPtr;
void HR(HRESULT hr) { Check(SUCCEEDED(hr),"DX11/WIC operation"); }
void PNG(IWICImagingFactory* factory,const std::filesystem::path& path,unsigned char red) {
    ComPtr<IWICStream> stream;HR(factory->CreateStream(&stream));HR(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;HR(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
    HR(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;ComPtr<IPropertyBag2> options;HR(encoder->CreateNewFrame(&frame,&options));
    HR(frame->Initialize(options.Get()));HR(frame->SetSize(1,1));GUID format=GUID_WICPixelFormat32bppBGRA;HR(frame->SetPixelFormat(&format));
    BYTE pixel[]{0,0,red,255};HR(frame->WritePixels(1,4,4,pixel));HR(frame->Commit());HR(encoder->Commit());
}
void LoadLogging() {
    using namespace I4;
    const std::string unresolved="I4 unresolved/unloaded plugin (possibly merged; no remapping): ";
    const std::vector<std::string> warnings{
        "Z.esp: invalid iconSource", "RotolsMoreIcons.esp: Plugin file not found: LostGrimoire.esp",
        "A.esp: Plugin file not found: Other.esl", "Z.esp: invalid iconSource",
        "RotolsMoreIcons.esp: Plugin file not found: LostGrimoire.esp",
        "B.esp: Plugin file not found: lostgrimoire.ESP", "A.esp: another warning",
        unresolved+"LOSTGRIMOIRE.esp", unresolved+"Other.esl"};
    const auto original=warnings;
    const auto counted=CountLoadWarnings(warnings);
    Check(counted.size()==7 && counted[0].text==warnings[0] && counted[0].count==2 &&
        counted[1].text==warnings[1] && counted[1].count==2 && counted[2].text==warnings[2] && counted[2].count==1,
        "exact duplicate warnings counted in first occurrence order");
    const auto summary=SummarizeLoadWarnings(warnings);
    Check(summary.missingReferences==4 && summary.missingPlugins==2,
        "references aggregated across files and plugin case without counting unresolved diagnostics twice");
    Check(summary.warnings==std::vector<std::string>{
        "Z.esp: invalid iconSource (x2)",
        "missing/unloaded plugin: LostGrimoire.esp; references=3 (possibly merged; no remapping)",
        "missing/unloaded plugin: Other.esl; references=1 (possibly merged; no remapping)",
        "A.esp: another warning"},
        "one line per missing plugin retains first occurrence order and unrelated warnings");
    Check(warnings==original,"presentation leaves original warnings unchanged");
    const std::vector<std::string> diagnosticFirst{unresolved+"B.esp",unresolved+"Orphan.esp",
        "A.esp: Plugin file not found: B.esp",unresolved+"B.esp"};
    const auto early=SummarizeLoadWarnings(diagnosticFirst);
    Check(early.missingReferences==1 && early.missingPlugins==2 && early.warnings.size()==2 &&
        early.warnings[0]=="missing/unloaded plugin: B.esp; references=1 (possibly merged; no remapping)" &&
        early.warnings[1]=="missing/unloaded plugin: Orphan.esp; references=0 (possibly merged; no remapping)",
        "diagnostic-first and diagnostic-only cases do not invent references");
    const auto empty=SummarizeLoadWarnings({});
    Check(empty.warnings.empty() && empty.missingReferences==0 && empty.missingPlugins==0,"empty report stays empty");
    const std::vector<std::string> malformed{"A.esp: Plugin file not found: ",unresolved};
    const auto unchanged=SummarizeLoadWarnings(malformed);
    Check(unchanged.warnings==malformed && unchanged.missingPlugins==0,"empty plugin names remain ordinary warnings");
    const std::pair<std::string,std::string> file{"Fixture.esp",R"({"rules":[
        {"match":{"formId":"Missing.esp|123"},"assign":{"iconLabel":"first"}},
        {"match":{"formId":"Missing.esp|456"},"assign":{"iconLabel":"second"}}]})"};
    LoadOptions options;options.sourceExists=[](auto){return true;};
    options.pluginPrefix=[](auto)->std::optional<std::uint32_t>{return {};};
    const auto report=Load(std::span(&file,1),options);const auto before=report;
    const auto loaded=SummarizeLoadWarnings(report.warnings);
    Check(report.files==1 && report.rules==2 && report.rejected==0 && report.warnings.size()==2 &&
        loaded.missingReferences==2 && loaded.missingPlugins==1 && loaded.warnings.size()==1,
        "real LoadReport retains two rules and raw warnings while log contains one missing plugin");
    Check(report.files==before.files && report.rules==before.rules && report.rejected==before.rejected && report.warnings==before.warnings,
        "log summary does not mutate LoadReport contents or counters");
    std::cout<<"load logging: 9 raw warnings -> "<<summary.warnings.size()<<" lines, "
        <<summary.missingReferences<<" references to "<<summary.missingPlugins<<" missing plugins\n";
}
void Policy() {
    using namespace I4;
    Check(TextureEdge(32)==64 && TextureEdge(32.1f)==128 && TextureEdge(64)==128 && TextureEdge(65)==256 && TextureEdge(1000)==256,"edge tiers");
    Check(MipBytes(64)==21844 && MipBytes(128)==87380 && MipBytes(256)==349524,"exact full mip budget");
    TextureKey a{"a","Case\n",Variant::Mask,64},b=a,c=a,d=a;
    b.label="case\n";c.variant=Variant::Original;d.edge=128;
    Check(std::set<TextureKey>{a,b,c,d}.size()==4,"cache distinguishes label case/newline, variant, edge");
    Check(Identity(Icon{"A\\B.SWF","Case\n"})==IconKey{"a/b.swf","Case\n"},"source normalized, label unchanged");
    TextureBudget policy;std::vector<TextureKey> evicted;const auto limit=MipBytes(64)*2;
    Check(policy.Admit(a,limit,evicted) && policy.Admit(b,limit,evicted),"two entries fit");
    policy.Touch(a);Check(policy.Admit(c,limit,evicted) && evicted==std::vector<TextureKey>{b},"LRU touch changes victim");
    policy.Pin({{a.source,a.label}});evicted.clear();
    Check(!policy.Admit(b,limit,evicted) && policy.Contains(a) && policy.Contains(c),"both pinned variants survive budget pressure");
    policy.Pin({});Check(policy.Admit(d,MipBytes(128),evicted) && policy.Used()==MipBytes(128),"size change evicts idle entries, includes mips");
    policy.Pin({{d.source,d.label}},64);evicted.clear();
    Check(policy.Admit(a,MipBytes(64),evicted) && !policy.Contains(d),"old resolution is unpinned after size change");
    policy.Clear();Check(policy.Used()==0,"device reset budget clear");
    policy.Admit(a,limit*2,evicted);policy.Admit(b,limit*2,evicted);policy.Admit(c,limit*2,evicted);policy.Touch(b);evicted.clear();
    Check(policy.Trim(limit,evicted) && evicted==std::vector<TextureKey>{a} && policy.Contains(b) && policy.Contains(c),"budget shrink evicts least recently used entry without admitting a new one");
    policy.Pin({{b.source,b.label}});evicted.clear();
    Check(!policy.Trim(0,evicted) && policy.Contains(b) && !policy.Contains(c),"trim evicts all idle entries but preserves snapshot pins above budget");
    auto config=RuntimeConfig{};dataAvailable=true;Icon icon{"a","b",{},IconOrigin::Default};
    Check(Eligible(icon) && !Eligible(icon,true),"Auto and theme Off");
    config.icons=false;Check(!Eligible(icon,false,config),"global Off");config.icons=true;config.scope=IconScope::Rule;
    Check(!Eligible(icon,false,config),"rule-only scope");icon.origin=IconOrigin::Rule;Check(Eligible(icon,false,config),"rule scope includes explicit rules");
    dataAvailable=false;Check(!Eligible(icon),"no JSON disables even explicit snapshot icons");
    IconLibrary::Image image;const auto ink=0x7F123456u;
    Check(IconLibrary::ImageColor(image,ink)==ink,"Theme retains exact hover/selection ink");
    image.color=0x112233;Check(IconLibrary::ImageColor(image,ink)==0x7F332211,"I4 RGB conversion preserves UI alpha");
    image.color.reset();image.tint=false;Check(IconLibrary::ImageColor(image,ink)==0x7FFFFFFF,"Original only multiplies UI alpha");
}
void RuntimeSettings() {
    const auto root=std::filesystem::path("build/i4-phase3/t5/runtime-settings");std::filesystem::create_directories(root);
    const auto file=root/"FavoriteWheel.ini";SetSettingsPath(file.string());
    {std::ofstream f(file);f<<"[Display]\nI4Icons=0\nI4IconColor=i4\nI4IconScope=rule\nI4TextureBudgetMB=64\n[General]\nI4Debug=1\n";}
    LoadSettings();auto runtime=I4::Runtime();
    Check(!runtime.icons && runtime.color==I4::ColorMode::I4 && runtime.scope==I4::IconScope::Rule && runtime.textureBudgetMB==64 && runtime.debug,"Load publishes every I4 runtime field");
    BeginSettings();DefaultSettings();Check(!I4::Runtime().icons && I4::Runtime().debug,"Defaults draft is not applied early");RevertSettings();
    BeginSettings();auto draft=Config();draft.i4Icons=true;draft.i4IconColor="Original";draft.i4IconScope="All";draft.i4TextureBudgetMB=8;draft.i4Debug=false;EditSettings(draft);
    Check(!I4::Runtime().icons && I4::Runtime().textureBudgetMB==64,"Edit does not publish runtime config");
    SetSettingsPath(root.string());Check(!SaveSettings() && !I4::Runtime().icons && I4::Runtime().debug,"Failed Apply preserves runtime config");
    SetSettingsPath(file.string());Check(SaveSettings(),"Apply runtime settings");runtime=I4::Runtime();
    Check(runtime.icons && runtime.color==I4::ColorMode::Original && runtime.scope==I4::IconScope::All && runtime.textureBudgetMB==8 && !runtime.debug,"Successful Apply publishes all fields atomically");
    auto baseline=Config(),external=baseline;external.i4Icons=false;external.i4Debug=true;external.i4TextureBudgetMB=128;
    Check(SaveExternalSettings(baseline,external) && !I4::Runtime().icons && I4::Runtime().debug && I4::Runtime().textureBudgetMB==128,"Framework Apply shares runtime publication");
    external.i4Icons=true;external.theme="invalid\nvalue";
    Check(!SaveExternalSettings(Config(),external) && !I4::Runtime().icons,"Failed Framework Apply retains active I4 config");
    BeginSettings();DefaultSettings();Check(SaveSettings(),"Restore default runtime settings");
}
void Diagnostics() {
    using namespace I4;
    const std::vector<std::pair<std::string,std::string>> files{
        {"First.esp",R"({"rules":[{"match":{"formType":"Weapon"},"assign":{"subType":2}},{"match":{},"assign":{}},{"match":{"subType":2},"assign":{"iconLabel":"first","iconColor":"0x123456"}}]})"},
        {"Last.esl",R"({"rules":[{"match":{"subType":2},"assign":{"iconSource":"test.swf","iconLabel":"last"}}]})"}};
    LoadOptions options;options.sourceExists=[](auto){return true;};
    Check(Load(files,options).rules==3,"diagnostic fixture preserves rejected-rule indices");
    Entry a;a.Set("formType",41);a.Set("formId",0xFE123456);a.Set("subType",1);auto b=a;
    const auto plain=Resolve(a);std::vector<RuleMatch> matches;
    const auto traced=Resolve(b,&matches);
    Check(a.fields==b.fields && plain.source==traced.source && plain.label==traced.label && plain.color==traced.color && plain.origin==traced.origin,"optional trace does not change Resolve results or mutations");
    Check(matches.size()==3 && matches[0].file=="First.json" && matches[0].index==0 && matches[0].info && matches[1].index==2 && matches[2].file=="Last.json","trace identifies contributing files and original zero-based rule indices");
    Check(traced.source=="test.swf" && traced.label=="last" && traced.origin==IconOrigin::Rule,"later rule still overrides earlier icon assignment");
    const auto line=FormatResolveDebug(0xFE123456,"Sword\nName",traced,matches);
    Check(line.find("formId=FE123456")!=line.npos && line.find("Sword\\nName")!=line.npos && line.find("source=\"test.swf\"")!=line.npos && line.find("label=\"last\"")!=line.npos && line.find("color=")!=line.npos && line.find("origin=Rule")!=line.npos && line.find("First.json#2")!=line.npos && line.find("Last.json#0")!=line.npos && line.find('\n')==line.npos,"debug result contains requested fields on exactly one line");
    Entry empty;const auto none=Resolve(empty,&matches);Check(matches.empty() && none.origin==IconOrigin::None,"each trace clears previous matches");
    Entry weapon;weapon.Set("formType",41);weapon.Set("subType",1);Load({});
    const auto fallback=Resolve(weapon,&matches);
    Check(fallback.origin==IconOrigin::Default && matches.empty() && FormatResolveDebug(1,"Default",fallback,matches).find("origin=Default rules=[]")!=std::string::npos,"default icon with no matched rule is explicit in diagnostics");
}
void Integration() {
    HR(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level;
    HR(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context));
    ComPtr<IWICImagingFactory> factory;HR(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    const auto root=std::filesystem::path("build/i4-phase3/fixtures");std::filesystem::create_directories(root/"Themes");
    PNG(factory.Get(),root/"form.png",40);PNG(factory.Get(),root/"map.png",80);PNG(factory.Get(),root/"category.png",120);
    {std::ofstream f(root/"Themes/off.ini");f<<"Name=Off fixture\nI4Icons=Off\n";}
    {std::ofstream f(root/"Themes/auto.ini");f<<"Name=Auto fixture\n";}
    LoadResources(root.string());
    Check(std::ranges::any_of(Themes(),[](const Theme& t){return t.id=="off" && t.i4IconsOff;}),"theme INI Off parsed");
    Check(std::ranges::any_of(Themes(),[](const Theme& t){return t.id=="auto" && !t.i4IconsOff;}),"omitted theme token is Auto");
    {std::ofstream f(root/"icons.ini");f<<"[Icons]\nform=form.png|original\nmap=map.png|original\nsword=category.png|tint\nmagic=category.png|tint\n[Forms]\nSkyrim.esm|1=form\n[I4]\nA.SWF|icon=map\na.swf|icon =form\na.swf|tinted=sword\n";}
    Theme theme;theme.iconPack=(root/"icons.ini").string();std::array themes{theme};
    Check(IconLibrary::Initialize(device.Get(),context.Get(),themes).textures==3,"three independent pack textures");
    std::deque<std::function<void()>> tasks;std::vector<std::string> warnings;unsigned reads=0;const auto mainThread=std::this_thread::get_id();
    const auto fixture=Simple().v;
    I4Textures::Configure([&](const std::string& source)->std::optional<std::vector<std::uint8_t>> {
        Check(std::this_thread::get_id()==mainThread,"resource bytes read only in scheduled game task");++reads;
        if(source=="missing.swf")return {};
        if(source=="bad.swf")return std::vector<std::uint8_t>{1,2,3,4};
        return fixture;
    },[&](std::function<void()> task){tasks.push_back(std::move(task));},[&](const std::string& s){warnings.push_back(s);});
    I4Textures::Initialize(device.Get(),context.Get());I4Textures::BeginFrame(32);I4::dataAvailable=true;
    std::vector<I4::Icon> icons;
    for(int i=0;i<6;++i)icons.push_back({std::string(1,char('a'+i))+".swf","icon",0x112233,I4::IconOrigin::Rule});
    I4Textures::RequestSnapshot(icons);Check(reads==0 && tasks.size()==1,"snapshot only schedules reads");
    Item item;item.icon=IconKind::Sword;item.i4=icons[0];
    const auto mapped=IconLibrary::Find(theme,item);Check(mapped.view && !mapped.tint,"I4 pack replacement works before SWF ready");
    item.i4=icons[1];const auto category=IconLibrary::Find(theme,item);Check(category.view && category.tint,"pending SWF immediately falls back to category");
    float maximumIconPixels=32;
    auto step=[&] {
        const auto before=reads;
        auto current=std::move(tasks);tasks.clear();for(auto& task:current)task();
        Check(reads-before<=2,"at most two SWF reads per dispatched frame");
        I4Textures::BeginFrame(maximumIconPixels);Check(I4Textures::Stats().uploaded<=4,"at most four texture uploads per frame");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    };
    auto wait=[&](auto predicate) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(!predicate() && std::chrono::steady_clock::now()<deadline)step();
        Check(predicate(),"background pipeline completes before timeout");
    };
    while(reads<6) {
        const auto before=reads;auto current=std::move(tasks);tasks.clear();for(auto& task:current)task();
        Check(reads>before && reads-before<=2,"batched game reads make bounded progress");I4Textures::RequestSnapshot(icons);
    }
    const auto readyDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(I4Textures::Stats().ready<8 && std::chrono::steady_clock::now()<readyDeadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Check(I4Textures::Stats().ready==8,"worker backpressure caps completed bitmaps at eight");
    I4Textures::BeginFrame(32);Check(I4Textures::Stats().uploaded==4 && I4Textures::Stats().textures==4,"nonempty backlog uploads exactly four in one frame");
    wait([&]{return I4Textures::Stats().textures>=12;});Check(reads==6,"source bytes cached once for two variants");
    item.i4=icons[0];item.iconForm=IconLibrary::FormKey("Skyrim.esm",1);
    auto form=IconLibrary::Find(theme,item);Check(form.view!=mapped.view && !form.tint,"level 1 Forms overrides I4 map");
    item.iconForm.clear();Check(IconLibrary::Find(theme,item).view==mapped.view,"level 2 I4 pack overrides Rule");
    item.i4=icons[1];auto rule=IconLibrary::Find(theme,item);Check(rule.view && rule.view!=category.view,"level 3 Rule overrides category");
    auto sample=[&](ID3D11ShaderResourceView* view) {
        ComPtr<ID3D11Resource> resource;view->GetResource(&resource);ComPtr<ID3D11Texture2D> texture;HR(resource.As(&texture));
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);Check(desc.Width==64 && desc.MipLevels==7,"GPU texture has complete mip chain");
        desc.MipLevels=1;desc.BindFlags=0;desc.MiscFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;HR(device->CreateTexture2D(&desc,nullptr,&staging));context->CopySubresourceRegion(staging.Get(),0,0,0,0,texture.Get(),0,nullptr);
        D3D11_MAPPED_SUBRESOURCE mapped{};HR(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
        const auto pixel=static_cast<const BYTE*>(mapped.pData)+32*mapped.RowPitch+32*4;
        std::array<unsigned,4> rgba{pixel[0],pixel[1],pixel[2],pixel[3]};context->Unmap(staging.Get(),0);return rgba;
    };
    Check(sample(rule.view)==std::array<unsigned,4>{255,255,255,255},"worker mask remains white after GPU upload");
    item.i4.origin=I4::IconOrigin::Default;Check(IconLibrary::Find(theme,item).view==category.view,"level 4 category overrides Default");
    item.icon=IconKind::Restoration;Check(IconLibrary::Find(theme,item).view==category.view,"magic fallback precedes Default");
    Theme plain;Check(IconLibrary::Find(plain,item).view==rule.view,"level 5 Default without pack");
    item.i4={};Check(!IconLibrary::Find(plain,item).view,"level 6 built-in fallback");
    item.i4=icons[0];item.i4.label="icon ";Check(IconLibrary::Find(theme,item).view==form.view,"I4 label trailing space preserved");
    item.icon=IconKind::Sword;item.i4.label="Icon";Check(IconLibrary::Find(theme,item).view==category.view,"I4 label case significant");
    item.i4=icons[0];theme.i4IconsOff=true;Check(IconLibrary::Find(theme,item).view==category.view,"theme Off skips I4 maps and textures");
    theme.i4IconsOff=false;I4::dataAvailable=false;Check(IconLibrary::Find(theme,item).view==category.view,"no data preserves category behavior");
    I4::dataAvailable=true;auto config=I4::Runtime();config.icons=false;I4::runtimeConfig=config;
    Check(IconLibrary::Find(theme,item).view==category.view,"global Off preserves category behavior");config.icons=true;I4::runtimeConfig=config;
    item.action=ActionKind::SaveOutfit;Check(!IconLibrary::Find(plain,item).view,"function buttons ignore I4");item.action=ActionKind::Favorite;
    item.i4=icons[1];config.color=I4::ColorMode::I4;I4::runtimeConfig=config;
    Check(IconLibrary::Find(plain,item).color==0x112233,"I4 color propagated");
    item.i4.color.reset();Check(!IconLibrary::Find(plain,item).color && IconLibrary::Find(plain,item).tint,"I4 no color uses theme ink");
    config.color=I4::ColorMode::Original;I4::runtimeConfig=config;
    Check(!IconLibrary::Find(plain,item).tint && IconLibrary::Find(plain,item).view!=rule.view,"Original selects distinct original texture");
    Check(sample(IconLibrary::Find(plain,item).view)==std::array<unsigned,4>{240,40,20,255},"worker original retains authored RGB");
    item.i4=icons[0];Check(!IconLibrary::Find(theme,item).tint && !IconLibrary::Find(theme,item).color,"pack replacement retains own color policy");
    // AC-6: every color mode retains all six priority levels and pack tint policy.
    const auto renderCount=I4Textures::Stats().renders;
    const auto ink=0x7F123456u,hover=0xCC987654u;
    for(const auto mode:{I4::ColorMode::Theme,I4::ColorMode::I4,I4::ColorMode::Original}) {
        config.color=mode;config.scope=I4::IconScope::All;I4::runtimeConfig=config;
        item.i4=icons[0];item.iconForm=IconLibrary::FormKey("Skyrim.esm",1);
        Check(IconLibrary::Find(theme,item).view==form.view,"AC-6 Forms priority in every color mode");
        item.iconForm.clear();auto replacement=IconLibrary::Find(theme,item);
        Check(replacement.view==mapped.view && !replacement.tint && !replacement.color,"AC-6 I4 map authored color overrides every mode");
        item.i4.label="tinted";replacement=IconLibrary::Find(theme,item);
        Check(replacement.view==category.view && replacement.tint && !replacement.color && IconLibrary::ImageColor(replacement,hover)==hover,"AC-6 tinted I4 pack replacement follows theme ink in every mode");
        item.i4=icons[1];auto image=IconLibrary::Find(theme,item);
        const auto expected=mode==I4::ColorMode::Theme?ink:mode==I4::ColorMode::I4?0x7F332211u:0x7FFFFFFFu;
        Check(image.view!=category.view && IconLibrary::ImageColor(image,ink)==expected,"AC-6 Rule priority and exact RGB/alpha per mode");
        Check(IconLibrary::ImageColor(image,hover)==(mode==I4::ColorMode::Theme?hover:mode==I4::ColorMode::I4?0xCC332211u:0xCCFFFFFFu),"AC-6 selected and hovered theme ink vs fixed color");
        item.i4.origin=I4::IconOrigin::Default;
        Check(IconLibrary::Find(theme,item).view==category.view && IconLibrary::ImageColor(IconLibrary::Find(theme,item),hover)==hover,"AC-6 tinted category precedes Default and follows theme ink");
        Check(IconLibrary::Find(plain,item).view==image.view,"AC-6 Default used without pack in every mode");
        item.i4={};Check(!IconLibrary::Find(plain,item).view,"AC-6 built-in fallback in every mode");
    }
    Check(I4Textures::Stats().renders==renderCount,"color changes reuse prepared variants without clearing or rasterizing");
    I4::runtimeConfig=I4::RuntimeConfig{};I4Textures::RequestSnapshot(icons);
    I4Textures::Reset();Check(I4Textures::Stats().textures==0 && I4Textures::Stats().gpuBytes==0,"reset clears GPU cache and mip accounting");
    I4Textures::Initialize(device.Get(),context.Get());wait([&]{return I4Textures::Stats().textures>=12;});
    Check(reads==6,"device recreation reuses source bytes and requests pinned icons");
    const auto stableReads=reads;const auto stableRenders=I4Textures::Stats().renders;
    for(int i=0;i<10;++i)step();Check(reads==stableReads && I4Textures::Stats().pending==0 && I4Textures::Stats().renders==stableRenders,"steady state no reads or rasterization");
    Check(I4Textures::Stats().parses==6,"device recreation reuses immutable parsed movies");
    icons.push_back({"missing.swf","icon",{},I4::IconOrigin::Rule});icons.push_back({"bad.swf","icon",{},I4::IconOrigin::Rule});
    icons.push_back({"a.swf","absent",{},I4::IconOrigin::Rule});I4Textures::RequestSnapshot(icons);
    wait([&]{return warnings.size()>=3;});const auto warningCount=warnings.size();const auto readCount=reads;
    I4Textures::RequestSnapshot(icons);for(int i=0;i<12;++i)step();
    Check(warnings.size()==warningCount && reads==readCount,"failed source/label warned once and never retried");
    // All entries, including I4 mappings, share the existing per-pack 512 limit.
    {std::ofstream f(root/"limit.ini");f<<"[Icons]\nmap=map.png\n[I4]\n";for(int i=0;i<511;++i)f<<"a.swf|label"<<i<<"=map\n";f<<"a.swf|overflow=map\n";}
    theme.iconPack=(root/"limit.ini").string();themes={theme};const auto limited=IconLibrary::Initialize(device.Get(),context.Get(),themes);
    Check(!limited.warnings.empty(),"512 entry limit diagnosed");item.i4={"A.SWF","label510",{},I4::IconOrigin::Default};
    Check(IconLibrary::Find(theme,item).view!=nullptr,"entry 512 accepted");item.i4.label="overflow";
    Check(!IconLibrary::Find(theme,item).view,"entry 513 rejected");
    std::cout<<"pipeline reads="<<reads<<" warnings="<<warnings.size()<<" gpu_bytes="<<I4Textures::Stats().gpuBytes<<'\n';
    // Scope changes request previously excluded defaults from the retained snapshot.
    auto scoped=icons;scoped.push_back({"scope-default.swf","icon",{},I4::IconOrigin::Default});
    config.scope=I4::IconScope::Rule;config.color=I4::ColorMode::Theme;I4::runtimeConfig=config;
    I4Textures::RequestSnapshot(scoped);step();Check(!I4Textures::Find(scoped.back()).view,"Rule scope excludes default textures");
    const auto scopeReads=reads;config.scope=I4::IconScope::All;I4::runtimeConfig=config;
    wait([&]{return I4Textures::Find(scoped.back()).view!=nullptr;});
    Check(reads==scopeReads+1,"All scope requests excluded defaults without a new snapshot");
    config.scope=I4::IconScope::Rule;I4::runtimeConfig=config;step();
    Check(!I4Textures::Find(scoped.back()).view && I4Textures::Find(scoped.front()).view,"scope applies on next frame while Rule stays ready");
    // A small real GPU budget exercises the production admission/upload path.
    const std::vector<I4::Icon> pressure(icons.begin(),icons.begin()+6);
    const auto victim=pressure.back();
    config=I4::Runtime();config.textureBudgetMB=1;I4::runtimeConfig=config;
    maximumIconPixels=128;
    const auto beforePressure=I4Textures::Stats().renders;
    I4Textures::RequestSnapshot(pressure);
    wait([&]{return I4Textures::Stats().renders>=beforePressure+12 && I4Textures::Stats().pending==0 && I4Textures::Stats().ready==0;});
    step();
    Check(!I4Textures::Find(victim).view,"budget pressure leaves the last icon unavailable");
    const auto pressureWarnings=warnings.size();const auto pressureRenders=I4Textures::Stats().renders;
    for(int i=0;i<20;++i){I4Textures::RequestSnapshot(pressure);step();}
    Check(warnings.size()==pressureWarnings && I4Textures::Stats().renders==pressureRenders,"unchanged snapshot does not retry transient failures every frame");
    const auto bothVariants=[&](const I4::Icon& icon) {
        auto mode=I4::Runtime();mode.color=I4::ColorMode::Theme;I4::runtimeConfig=mode;
        const bool mask=I4Textures::Find(icon).view!=nullptr;
        mode.color=I4::ColorMode::Original;I4::runtimeConfig=mode;
        const bool original=I4Textures::Find(icon).view!=nullptr;
        mode.color=I4::ColorMode::Theme;I4::runtimeConfig=mode;return mask && original;
    };
    I4Textures::RequestSnapshot(std::span(&victim,1));
    wait([&]{return bothVariants(victim);});
    Check(I4Textures::Stats().gpuBytes<=1024*1024,"smaller snapshot restores both variants within budget without Reset");
    Check(warnings.size()==pressureWarnings,"successful retry does not duplicate warnings");
    // Re-establish pressure, then reduce only the edge (no snapshot/Reset).
    const auto beforeSecondPressure=I4Textures::Stats().renders;
    I4Textures::RequestSnapshot(pressure);
    wait([&]{return I4Textures::Stats().renders>beforeSecondPressure && I4Textures::Stats().pending==0 && I4Textures::Stats().ready==0;});
    for(int i=0;i<20;++i)step();
    const auto unavailable=std::ranges::find_if(pressure,[&](const auto& icon){return !bothVariants(icon);});
    Check(unavailable!=pressure.end(),"renewed pressure blocks at least one variant");
    const auto edgeVictim=*unavailable;const auto beforeShrinkWarnings=warnings.size();
    const auto beforeShrinkRenders=I4Textures::Stats().renders;
    for(int i=0;i<20;++i)step();
    Check(I4Textures::Stats().renders==beforeShrinkRenders,"renewed budget failure settles without per-frame retry");
    maximumIconPixels=32;
    wait([&]{return std::ranges::all_of(pressure,bothVariants);});
    Check(bothVariants(edgeVictim) && I4Textures::Stats().gpuBytes<=1024*1024,"smaller edge restores failed icons without Reset or snapshot change");
    Check(warnings.size()==beforeShrinkWarnings,"edge retry preserves once-per-identity warning deduplication");
    for(const auto& icon:pressure) {
        const auto message="I4 texture upload/budget failed: "+icon.source+" | "+icon.label;
        Check(std::ranges::count(warnings,message)<=1,"upload warning is emitted once per identity across retry epochs");
    }
    // Budget-only changes through real Settings Apply (8..128 MiB), without Reset.
    std::vector<I4::Icon> budgetIcons;
    for(int i=0;i<16;++i)budgetIcons.push_back({"budget-"+std::to_string(i)+".swf","icon",{},I4::IconOrigin::Rule});
    auto applyBudget=[&](int mb) {
        const auto before=Config();auto after=before;after.i4TextureBudgetMB=mb;
        Check(SaveExternalSettings(before,after),"apply texture budget through settings");
    };
    maximumIconPixels=128;applyBudget(8);
    const auto beforeBudgetRenders=I4Textures::Stats().renders;
    I4Textures::RequestSnapshot(budgetIcons);
    wait([&]{return I4Textures::Stats().renders>=beforeBudgetRenders+32 && I4Textures::Stats().pending==0 && I4Textures::Stats().ready==0;});
    Check(!std::ranges::all_of(budgetIcons,bothVariants),"8 MiB pressure blocks some variants");
    applyBudget(32);
    wait([&]{return std::ranges::all_of(budgetIcons,bothVariants);});
    const auto largeBytes=I4Textures::Stats().gpuBytes;
    Check(largeBytes>8*1024*1024,"larger budget retries previously blocked textures with unchanged snapshot and edge");
    applyBudget(8);step();
    Check(std::ranges::all_of(budgetIcons,bothVariants) && I4Textures::Stats().gpuBytes>=32*I4::MipBytes(256),"current snapshot pins survive a budget decrease even above limit");
    const auto budgetVictim=budgetIcons.back();I4Textures::RequestSnapshot(std::span(&budgetVictim,1));step();
    Check(I4Textures::Stats().gpuBytes<=8*1024*1024 && bothVariants(budgetVictim),"reduced budget trims unpinned GPU textures even without pending uploads");
    applyBudget(32);
    I4::runtimeConfig=I4::RuntimeConfig{};
    I4Textures::Shutdown();IconLibrary::Reset();factory.Reset();CoUninitialize();
}
void ShutdownReads() {
    std::deque<std::function<void()>> tasks;
    std::vector<std::string> warnings;
    auto scheduler=[&](std::function<void()> task){tasks.push_back(std::move(task));};
    auto logger=[&](const std::string& warning){warnings.push_back(warning);};
    std::promise<void> entered,release;
    auto released=release.get_future().share();
    I4Textures::Configure([&](const auto&)->std::optional<std::vector<std::uint8_t>> {
        entered.set_value();released.wait();return {};
    },scheduler,logger);
    const I4::Icon inflight{"shutdown-inflight.swf","icon",{},I4::IconOrigin::Rule};
    I4Textures::RequestSnapshot(std::span(&inflight,1));
    Check(tasks.size()==1,"in-flight fixture schedules a game task");
    auto task=std::move(tasks.front());tasks.pop_front();
    auto started=entered.get_future();
    std::jthread gameTask([task=std::move(task)]{task();});
    started.wait();
    I4Textures::Shutdown(); // Completes while the resource read is still blocked.
    release.set_value();gameTask.join();
    I4Textures::BeginFrame(32);
    Check(warnings.empty(),"read returning after Shutdown discards failure instead of publishing a warning");
    Check(I4Textures::Stats().pending==0 && I4Textures::Stats().ready==0,"late read does not enqueue work after Shutdown");
    unsigned reads=0;
    I4Textures::Configure([&](const auto&)->std::optional<std::vector<std::uint8_t>>{++reads;return {};},scheduler,logger);
    const I4::Icon queued{"shutdown-queued.swf","icon",{},I4::IconOrigin::Rule};
    I4Textures::RequestSnapshot(std::span(&queued,1));
    Check(tasks.size()==1,"shutdown fixture holds an unexecuted game task");
    I4Textures::Shutdown();
    task=std::move(tasks.front());tasks.pop_front();task();
    Check(reads==0,"queued game task after Shutdown does not invoke reader");
}
void Timing() {
    using namespace I4;
    const std::pair<std::string,std::string> json{"Fixture.esp",R"({"rules":[{"match":{"formType":"Weapon"},"assign":{"iconSource":"a.swf","iconLabel":"icon"}}]})"};
    LoadOptions options;options.sourceExists=[](auto){return true;};options.pluginPrefix=[](auto){return std::optional<std::uint32_t>{0x03000000};};
    Check(Load(std::span(&json,1),options).rules==1,"performance fixture loads its rule");std::size_t checksum=0;
    for(unsigned count:{100,500}) {
        std::vector<RawFields> raw(count);for(unsigned i=0;i<count;++i) {raw[i].formType=41;raw[i].formId=0x03000001+i;raw[i].weaponType=1;raw[i].keywords=Value::Keywords{"WeapTypeSword"};}
        double maximum=0,total=0;
        for(int pass=0;pass<21;++pass) {
            const auto start=std::chrono::steady_clock::now();
            for(const auto& fields:raw){auto e=Derive(fields);checksum+=Resolve(e).label.size();}
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            if(pass){maximum=std::max(maximum,ms);total+=ms;}
        }
        std::cout<<"AC-7 offline Derive+Resolve only: items="<<count<<" mean_ms="<<total/20<<" max_ms="<<maximum<<" (Collect requires game)\n";
    }
    Check(checksum>0,"benchmark results consumed");
}
}
int main() {
    try {RuntimeSettings();Diagnostics();LoadLogging();Policy();Integration();ShutdownReads();Timing();std::cout<<"I4IntegrationTests passed: "<<checks<<" checks\n";return 0;}
    catch(const std::exception& e) {I4Textures::Shutdown();std::cerr<<"I4IntegrationTests failed: "<<e.what()<<'\n';return 1;}
}

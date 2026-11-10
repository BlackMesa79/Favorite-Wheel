// Offline, read-only loose-file audit. Never opens Interface/ or artwork files.
#include "I4Rules.h"
#include "I4Entry.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
namespace fs=std::filesystem;
using Json=nlohmann::json;
using namespace Wheel::I4;
namespace {
std::string Read(const fs::path& p) {
    std::ifstream in(p,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot read: "+p.string());
    return {std::istreambuf_iterator<char>(in),{}};
}
std::string Lower(std::string s){for(char& c:s)if(c>='A'&&c<='Z')c+='a'-'A';return s;}
std::vector<std::string> Lines(std::string text){
    if(text.starts_with("\xEF\xBB\xBF"))text.erase(0,3);
    std::istringstream input(text);std::vector<std::string> result;std::string line;
    while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();result.push_back(line);}
    return result;
}
std::string Utf8(const fs::path& p){auto s=p.generic_u8string();return {s.begin(),s.end()};}
fs::path Path(const std::string& s){return fs::u8path(s);}
void Write(const fs::path& p,std::string_view text){std::ofstream out(p,std::ios::binary);if(!out)throw std::runtime_error("Cannot write output");out<<text;}
}
int main(int argc,char** argv)try {
    if(argc!=5 && argc!=6){std::cerr<<"Usage: I4RuleAudit <mods> <profile> <InventoryInjector.log> <output-directory> [Skyrim.ccc]\n";return 2;}
    const auto mods=Path(argv[1]),profile=Path(argv[2]),log=Path(argv[3]),out=Path(argv[4]);
    // Only the output directory is writable. Never write to any mod/profile/log path.
    fs::create_directories(out);
    std::map<std::string,fs::path> configs;
    std::vector<std::string> enabledMods;
    for(const auto& line:Lines(Read(profile/"modlist.txt"))){
        if(line.empty()||line[0]!='+')continue;
        enabledMods.push_back(line.substr(1));
        auto dir=mods/Path(line.substr(1))/"SKSE"/"Plugins"/"InventoryInjector";
        if(!fs::is_directory(dir))continue;
        // MO2 modlist lists highest priority first; first loose path wins.
        for(const auto& file:fs::directory_iterator(dir))if(file.is_regular_file()&&Lower(Utf8(file.path().extension()))==".json")
            configs.try_emplace(Lower(Utf8(file.path().filename())),file.path());
    }
    std::vector<std::string> plugins;
    std::set<std::string> enabled;
    // Skyrim's always-enabled masters may not be starred/included in plugins.txt.
    for(auto name:{"Skyrim.esm","Update.esm","Dawnguard.esm","HearthFires.esm","Dragonborn.esm"}){plugins.emplace_back(name);enabled.insert(Lower(name));}
    // Creation Club entries in Skyrim.ccc are implicitly enabled, absent from plugins.txt.
    if(argc==6)for(const auto& name:Lines(Read(Path(argv[5]))))
        if(!name.empty()&&name[0]!='#'&&enabled.insert(Lower(name)).second)plugins.push_back(name);
    for(const auto& line:Lines(Read(profile/"plugins.txt")))if(!line.empty()&&line[0]=='*'){
        auto name=line.substr(1);if(enabled.insert(Lower(name)).second)plugins.push_back(name);
    }
    // loadorder.txt includes the implicitly loaded masters/CC files as well.
    if(fs::is_regular_file(profile/"loadorder.txt")) {
        std::map<std::string,std::size_t> positions;
        for(const auto& name:Lines(Read(profile/"loadorder.txt")))
            if(!name.empty()&&name[0]!='#')positions.try_emplace(Lower(name),positions.size());
        std::stable_sort(plugins.begin(),plugins.end(),[&](const auto& a,const auto& b){
            auto rank=[&](const auto& n){auto it=positions.find(Lower(n));return it==positions.end()?positions.size():it->second;};
            return rank(a)<rank(b);
        });
    }
    std::vector<std::pair<std::string,std::size_t>> logged;
    std::map<std::string,std::size_t> loggedMissing;
    std::regex readPattern(R"(Read ([0-9]+) rules from (.*\.json))"),missingPattern(R"(Plugin file not found: (.*))");
    std::string version;
    for(const auto& line:Lines(Read(log))){
        std::smatch match;
        if(std::regex_search(line,match,readPattern))logged.emplace_back(match[2].str(),std::stoull(match[1].str()));
        if(std::regex_search(line,match,missingPattern))++loggedMissing[match[1].str()];
        if(line.find("InventoryInjector v")!=std::string::npos)version=line;
    }
    // Counts do not depend on runtime indices. Zero is a placeholder for KNOWN loaded
    // plugins, not an inferred prefix. Missing plugins still warn and resolve to zero.
    LoadOptions options;
    options.pluginPrefix=[&](std::string_view name)->std::optional<std::uint32_t>{return enabled.contains(Lower(std::string(name)))?std::optional<std::uint32_t>(0):std::nullopt;};
    // sourceExists deliberately omitted: BSA existence cannot be proven without assets.
    Json result={{"profile",Utf8(profile)},{"mods",Utf8(mods)},{"log",Utf8(log)},{"log_version",version},
        {"upstream","52cc4cb30f2ec09eba104606c8cdd17050c3a2bf"},{"enabled_mods",enabledMods.size()},
        {"enabled_plugins",plugins.size()},{"implicit_ccc",argc==6?argv[5]:"not supplied"},{"source_validation","assumed present; no artwork read or copied"},
        {"runtime_prefixes","zero placeholders for counts only; not runtime matching validation"},
        {"files",Json::array()},{"missing_plugin_warnings",Json::array()}};
    std::vector<std::pair<std::string,std::string>> input;
    std::map<std::string,std::size_t> counts;
    for(const auto& plugin:plugins){
        auto name=Path(plugin);name.replace_extension(".json");auto key=Lower(Utf8(name));
        auto it=configs.find(key);if(it==configs.end())continue;
        input.emplace_back(plugin,Read(it->second));
        const auto& file=input.back();auto report=Load(std::span(&file,1),options);
        counts[key]=report.rules;
        result["files"].push_back({{"plugin",plugin},{"json",Utf8(name)},{"path",Utf8(it->second)},
            {"parsed_files",report.files},{"rules",report.rules},{"rejected",report.rejected},{"warnings",report.warnings}});
    }
    auto all=Load(input,options);
    std::map<std::string,std::size_t> oursMissing;
    for(const auto& warning:all.warnings){auto pos=warning.find("Plugin file not found: ");if(pos!=std::string::npos)++oursMissing[warning.substr(pos+23)];}
    std::set<std::string> missingNames;
    for(const auto& [name,n]:loggedMissing)missingNames.insert(name);
    for(const auto& [name,n]:oursMissing)missingNames.insert(name);
    for(const auto& name:missingNames)result["missing_plugin_warnings"].push_back({{"plugin",name},{"log_count",loggedMissing[name]},
        {"audit_count",oursMissing[name]},{"enabled_in_profile",enabled.contains(Lower(name))}});
    Json comparison=Json::array();std::size_t mismatches=0;
    std::set<std::string> seen;
    for(const auto& [name,n]:logged){
        auto key=Lower(name);seen.insert(key);auto it=counts.find(key);
        const bool equal=it!=counts.end()&&it->second==n;
        comparison.push_back({{"file",name},{"log_rules",n},{"audit_rules",it==counts.end()?Json(nullptr):Json(it->second)},{"equal",equal}});
        if(!equal)++mismatches;
    }
    for(const auto& [key,count]:counts)if(!seen.contains(key)){comparison.push_back({{"file",key},{"log_rules",nullptr},{"audit_rules",count},{"equal",false}});++mismatches;}
    result["comparison"]=comparison;
    result["summary"]={{"log_files",logged.size()},{"audit_files",all.files},{"rules",all.rules},{"rejected",all.rejected},{"mismatches",mismatches}};
    // Also measure Resolve against the actual installed rule set; no game state assumed.
    result["performance"]=Json::array();
    for(int count:{100,500}){
        std::vector<Entry> originals;
        constexpr unsigned types[]={26,27,32,41,42,46,52,22,23};
        for(int i=0;i<count;++i){RawFields r;r.formType=types[i%9];r.formId=0x1000+i;r.armorType=0;r.partMask=4;r.weaponType=1;r.gemSize=3;r.soulSize=3;r.flags=0;r.school=20;r.resistance=41;r.keywords=Value::Keywords{"VendorItemGem","VendorItemClothing"};originals.push_back(Derive(r));}
        double total=0,maximum=0;std::size_t checksum=0;
        for(int pass=0;pass<21;++pass){auto entries=originals;auto start=std::chrono::steady_clock::now();for(auto& e:entries)checksum+=Resolve(e).label.size();auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(pass){total+=ms;maximum=std::max(maximum,ms);}}
        result["performance"].push_back({{"entries",count},{"mean_ms",total/20},{"max_ms",maximum},{"passes",20},{"checksum",checksum}});
    }
    std::ostringstream table;
    table<<"| JSON | I4 log | I4Rules | Result |\n|---|---:|---:|---|\n";
    for(const auto& row:comparison)table<<"| "<<row["file"].get<std::string>()<<" | "<<row["log_rules"].dump()<<" | "<<row["audit_rules"].dump()<<" | "<<(row["equal"].get<bool>()?"MATCH":"DIFFERENT")<<" |\n";
    Write(out/"audit.json",result.dump(2));Write(out/"comparison.md",table.str());
    std::cout<<result["summary"].dump()<<'\n'<<result["performance"].dump()<<'\n';
    return mismatches?1:0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 2;}

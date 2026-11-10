#include "I4Rules.h"
#include "I4Entry.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>
using namespace Wheel::I4;
using Json=nlohmann::json;
namespace {
int checks=0;
void Check(bool ok,const char* expr,int line) { ++checks; if(!ok) {std::cerr<<"I4RulesTests:"<<line<<": "<<expr<<'\n';std::exit(1);} }
#define CHECK(x) Check(bool(x),#x,__LINE__)
LoadOptions Options() {
    return {[](std::string_view p)->std::optional<std::uint32_t> {
        if(p=="Skyrim.esm") return 0;
        if(p=="Full.esp") return 0x03000000;
        if(p=="Light.esl"||p=="Flagged.esp") return 0xFE123000;
        return {};
    }, [](std::string_view s){return s!="missing.swf";}};
}
LoadReport Text(std::string s) {
    std::pair<std::string,std::string> file{"test.esp",std::move(s)};
    return Load(std::span(&file,1),Options());
}
LoadReport Rules(Json rules) {return Text(Json{{"rules",std::move(rules)}}.dump());}
Json Rule(Json match,Json assign={{"iconLabel","hit"}}) {return Json{{"match",std::move(match)},{"assign",std::move(assign)}};}
bool Hit(Entry e) {return Resolve(e).label=="hit";}
Entry Fields(std::initializer_list<std::pair<const std::string,Value>> fields) {Entry e;for(auto& [k,v]:fields)e.Set(k,v);return e;}
bool Is(const Entry& e,std::string_view k,Value v) {auto p=e.Get(k);return p&&*p==v;}
void Matching() {
    CHECK(Rules(Json::array({Rule({{"x",nullptr}})})).rules==1);
    CHECK(Hit({})); CHECK(Hit(Fields({{"x",Value{}}}))); CHECK(!Hit(Fields({{"x",0}}))); CHECK(!Hit(Fields({{"x",false}})));
    for(auto spec:{Json(true),Json(5),Json("X")}) {
        Rules(Json::array({Rule({{"x",spec}})}));
        Value v=spec.is_boolean()?Value(true):spec.is_number()?Value(5):Value("X");
        CHECK(Hit(Fields({{"x",v}})));CHECK(!Hit({}));
        CHECK(Hit(Fields({{"x",Value::Array{Value("wrong"),Value::Array{v}}}})));
        CHECK(!Hit(Fields({{"x",Value::Array{}}})));
    }
    Rules(Json::array({Rule({{"x",1}})})); CHECK(!Hit(Fields({{"x",true}}))); CHECK(!Hit(Fields({{"x","1"}})));
    // PropertyParser.cpp + Rule::AddProperty: a bare JSON array overwrites, last wins.
    Rules(Json::array({Rule({{"x",Json::array({1,2,3})}})}));
    CHECK(Hit(Fields({{"x",3}}))); CHECK(!Hit(Fields({{"x",1}})));
    Rules(Json::array({Rule({{"x",{{"anyOf",Json::array({1,2,{{"min",5},{"max",7}},nullptr})}}}})}));
    for(int i:{1,2,5,6,7}) CHECK(Hit(Fields({{"x",i}})));
    for(int i:{0,3,4,8}) CHECK(!Hit(Fields({{"x",i}})));
    CHECK(Hit({})); CHECK(!Hit(Fields({{"x","5"}})));
    Rules(Json::array({Rule({{"x",{{"anyOf",Json::array()}}}})})); CHECK(!Hit({}));
    Rules(Json::array({Rule({{"x",{{"anyOf",Json::array({1})},{"min",3}}}})}));
    CHECK(!Hit(Fields({{"x",1}}))); CHECK(Hit(Fields({{"x",4}})));
    Rules(Json::array({Rule({{"x",{{"max",3}}}})})); CHECK(Hit(Fields({{"x",3}}))); CHECK(!Hit(Fields({{"x",4}})));
    Rules(Json::array({Rule({{"x",{{"min",4},{"max",3}}}})})); CHECK(!Hit(Fields({{"x",3}})));
    for(auto name:{"keywords","effectKeywords"}) {
        Rules(Json::array({Rule({{name,"ExactCase"}})}));
        CHECK(Hit(Fields({{name,Value::Keywords{"ExactCase"}}})));CHECK(!Hit(Fields({{name,Value::Keywords{"exactcase"}}})));
        CHECK(!Hit({}));CHECK(!Hit(Fields({{name,"ExactCase"}})));
    }
    // Multiple match fields AND, unknown property names remain supported; $ names ignored.
    Rules(Json::array({Rule({{"x",1},{"y",2},{"$comment",false}})}));
    CHECK(Hit(Fields({{"x",1},{"y",2}}))); CHECK(!Hit(Fields({{"x",1}})));
}
void EnumsAndIds() {
    struct EnumCase {const char* name; const char* spelling; double value; const char* form;};
    const EnumCase cases[]={
        {"formType","aRmOr",26,"Armor"},{"weightClass","Heavy",1,"Armor"},
        {"weaponType","TwoHandAxe",6,"Weapon"},{"equipSlot","Skyrim.esm|13F44",0x13F44,"Weapon"},
        {"gemSize","Grand",5,"SoulGem"},{"status","Full",2,"SoulGem"},
        {"bookType","Book",0,"Book"},{"bookType","Note",-1,"Book"},
        {"teachesSkill","Marksman",8,"Book"},{"teachesSpell","Full.esp|ABC",0x03000ABC,"Book"},
        {"actorValue","HealRateMult",155,"Potion"},{"resistance","FireResist",41,"Spell"},
        {"school","Destruction",20,"Spell"},{"spellType","LesserPower",3,"Spell"},
        {"archetype","VampireLord",46,"Spell"},{"deliveryType","TargetLocation",4,"Spell"},
        {"castType","Concentration",2,"Spell"},{"subType","Body",3,"Armor"},
        {"subType","Note",1,"Book"},{"subType","HousePart",18,"MiscItem"},
        {"subType","Warhammer",7,"Weapon"},{"subType","Bolt",1,"Ammo"},
        {"subType","Poison",15,"Potion"},{"subType","Azura",6,"SoulGem"},
        {"iconColor","#AABBCC",0xAABBCC,"Weapon"}
    };
    auto form=[](std::string_view name){if(name=="Armor")return 26;if(name=="Weapon")return 41;if(name=="SoulGem")return 52;if(name=="Book")return 27;if(name=="Potion")return 46;if(name=="MiscItem")return 32;if(name=="Ammo")return 42;return 22;};
    for(auto c:cases) {
        Json match={{"formType",c.form}};match[c.name]=c.spelling;
        CHECK(Rules(Json::array({Rule(match)})).rules==1);
        Entry e=Fields({{"formType",form(c.form)}});e.Set(c.name,c.value);CHECK(Hit(e));e.Set(c.name,c.value+1);CHECK(!Hit(e));
    }
    for(auto name:{"weightClass","weaponType","teachesSkill","subType"}) {
        Rules(Json::array({Rule({{"formType","Armor"},{name,"Other"}})}));
        CHECK(Hit(Fields({{"formType",26}})));
    }
    Rules(Json::array({Rule({{"subType","custom"}})})); CHECK(Hit(Fields({{"subType","custom"}})));
    Rules(Json::array({Rule({{"formType","not-a-form"}})})); CHECK(!Hit(Fields({{"formType",0}}))); CHECK(Hit(Fields({{"formType",-1}})));
    const std::pair<const char*,unsigned> ids[]={{"Full.esp|000ABC",0x03000ABC},{"Light.esl|ABC",0xFE123ABC},{"Flagged.esp|ABC",0xFE123ABC},{"Light.esl|1001",0xFE124001},{"Missing.esp|123",0}};
    for(auto [text,id]:ids) {auto report=Rules(Json::array({Rule({{"formId",text}})}));CHECK(report.rules==1);CHECK(Hit(Fields({{"formId",id}})));CHECK(!Hit(Fields({{"formId",id+1}})));}
    CHECK(Rules(Json::array({Rule({{"formId","Missing.esp|123"}})})).warnings.size()==1);
    // Match bitfields by ANY common bit; numeric specs remain equality, not a mask.
    struct Flag {const char* form;const char* name;const char* flag;unsigned value;int type;};
    for(auto c:{Flag{"Book","flags","Read",8,27},Flag{"Ammo","flags","NonBolt",4,42},Flag{"Potion","flags","Poison",0x20000,46},Flag{"Spell","effectFlags","NoDeathDispel",0x10000000,22}}) {
        Rules(Json::array({Rule({{"formType",c.form},{c.name,c.flag}})}));CHECK(Hit(Fields({{"formType",c.type},{c.name,c.value|1}})));CHECK(!Hit(Fields({{"formType",c.type},{c.name,0}})));
        Rules(Json::array({Rule({{"formType",c.form},{c.name,c.value}})}));CHECK(!Hit(Fields({{"formType",c.type},{c.name,c.value|1}})));
    }
    Rules(Json::array({Rule({{"flags","Read"}})}));CHECK(Hit(Fields({{"flags","Read"}})));
}
void Parts() {
    constexpr unsigned precedence[]={2,1,3,4,7,8,9,5,6,11,13,0,12,10,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
    for(unsigned bit=0;bit<32;++bit) {
        Rules(Json::array({Rule({{"parts",bit+30}})}));CHECK(Hit(Fields({{"partMask",1u<<bit}})));CHECK(!Hit(Fields({{"partMask",0}})));CHECK(!Hit(Fields({{"partMask","wrong"}})));
        Rules(Json::array({Rule({{"mainPart",bit+30}})}));CHECK(Hit(Fields({{"partMask",1u<<bit}})));CHECK(!Hit(Fields({{"partMask",0}})));
    }
    for(unsigned i=0;i<32;++i) for(unsigned j=i+1;j<32;++j) {
        Rules(Json::array({Rule({{"mainPart",precedence[i]+30}})}));CHECK(Hit(Fields({{"partMask",(1u<<precedence[i])|(1u<<precedence[j])}})));
    }
    // JsonCpp orders member names: mainPart then parts, both write the partMask condition.
    Rules(Json::array({Rule({{"mainPart",32},{"parts",30}})}));CHECK(Hit(Fields({{"partMask",5}})));
    // anyOf ignores child property names, but outer name remains 'parts', exactly upstream.
    Rules(Json::array({Rule({{"parts",{{"anyOf",Json::array({30,32})}}}})}));CHECK(!Hit(Fields({{"partMask",5}})));CHECK(Hit(Fields({{"parts",5}})));
}
void ParsingAndValidation() {
    CHECK(Text("\xEF\xBB\xBF" R"(// comment
{"rules":[{"match":{"x":1,},"assign":{"iconLabel":"http://a/*b*/",}},],} ignored suffix)").rules==1);
    Entry e=Fields({{"x",1}});CHECK(Resolve(e).label=="http://a/*b*/");
    CHECK(Text(R"({"rules":[],"rules":[{"match":{"x":1},"assign":{"iconLabel":"hit"}}]})").rules==1);
    auto report=Rules(Json::array({Rule(Json::object()),Rule({{"x",1}},Json::object()),Rule({{"$x",1}}),Rule({{"x",Json::array()}}),Rule({{"x",1}},{{"x",nullptr},{"a",Json::array()},{"b",Json::object()}})}));
    CHECK(report.rejected==5);CHECK(report.rules==0);
    report=Rules(Json::array({Rule({{"x",1}},{{"iconSource","missing.swf"},{"iconLabel","x"}}),Rule({{"x",1}},{{"iconSource","missing.swf"},{"iconLabel","x"},{"iconColor","#001122"}}),Rule({{"x",1}},{{"iconSource",false},{"iconLabel","x"},{"keep",true}})}));
    CHECK(report.rules==2);CHECK(report.rejected==1);
    e=Fields({{"x",1},{"formType",27}});auto icon=Resolve(e);CHECK(icon.label=="default_book");CHECK(icon.color==0x1122);CHECK(icon.origin==IconOrigin::Rule);CHECK(!e.Get("iconSource"));CHECK(Is(e,"keep",true));
    Rules(Json::array({Rule({{"x",1}},{{"iconLabel","hit"}})}));e=Fields({{"x",1},{"iconSource","old.swf"}});icon=Resolve(e);CHECK(icon.source=="skyui/icons_item_psychosteve.swf");CHECK(Is(e,"iconSource",Value{}));
    // Explicit JSON null assignment is ignored, so validation still inserts undefined source.
    Rules(Json::array({Rule({{"x",1}},{{"iconSource",nullptr},{"iconLabel","hit"}})}));e=Fields({{"x",1}});CHECK(Resolve(e).label=="hit");
    Rules(Json::array({Rule({{"x",1}},{{"subType","Sword"},{"weightClass","Light"}})}));e=Fields({{"x",1}});Resolve(e);CHECK(Is(e,"subType","Sword"));CHECK(Is(e,"weightClass","Light"));
    Rules(Json::array({Rule({{"formType","Weapon"}},{{"subType","Sword"},{"$ignore",1}})}));e=Fields({{"formType",41}});CHECK(Resolve(e).label=="weapon_sword");CHECK(Is(e,"subTypeDisplay","$Sword"));CHECK(!e.Get("$ignore"));
    // Upstream WeightClassParser uses EquipType, not WeightClass; retain this quirk.
    Rules(Json::array({Rule({{"x",1}},{{"weightClass","Head"}})}));e=Fields({{"x",1}});Resolve(e);CHECK(Is(e,"weightClass",0));CHECK(Is(e,"weightClassDisplay","$Head"));
    // Source validation cached within a load, not carried across injected environments.
    int calls=0;auto options=Options();options.sourceExists=[&](std::string_view){++calls;return true;};
    std::pair<std::string,std::string> file{"a",Json{{"rules",Json::array({Rule({{"x",1}},{{"iconSource","a"}}),Rule({{"x",2}},{{"iconSource","a"}})})}}.dump()};
    Load(std::span(&file,1),options);CHECK(calls==1);
    options.sourceExists=[](std::string_view){return false;};CHECK(Load(std::span(&file,1),options).rejected==2);
    CHECK(Load(std::span(&file,1)).warnings.size()==2);
    // A file parse failure cannot discard other successfully parsed files.
    std::pair<std::string,std::string> files[]={{"bad","not json"},{"good",R"({"rules":[{"match":{"x":1},"assign":{"iconLabel":"hit"}}]})"}};
    report=Load(files,Options());CHECK(report.files==1);CHECK(report.rules==1);CHECK(report.warnings.size()==1);
}
void StagesAndOrder() {
    std::pair<std::string,std::string> files[]={
        {"first",Json{{"rules",Json::array({Rule({{"formType","Weapon"}},{{"subType","Sword"},{"iconLabel","first"}}),Rule({{"formId",123}},{{"iconLabel","id"}})})}}.dump()},
        {"second",Json{{"rules",Json::array({Rule({{"subType",1}},{{"iconLabel","hit"}})})}}.dump()}
    };
    CHECK(Load(files,Options()).rules==3);
    Entry e=Fields({{"formType",41},{"formId",123},{"subType",2},{"iconColor",7},{"iconLabel","old"}});
    auto icon=Resolve(e);CHECK(icon.label=="hit");CHECK(icon.color==0xA4A5BF);CHECK(icon.origin==IconOrigin::Rule);
    std::swap(files[0],files[1]);Load(files,Options());e=Fields({{"formType",41},{"formId",123},{"subType",2}});CHECK(Resolve(e).label=="id");
    // Earlier info writes affect later info matching and EVERY icon-stage re-match.
    Rules(Json::array({Rule({{"x",1}},{{"x",2},{"iconLabel","wrong"}}),Rule({{"x",2}},{{"x",3},{"iconLabel","wrong2"}}),Rule({{"x",3}},{{"iconLabel","hit"}})}));e=Fields({{"x",1}});CHECK(Resolve(e).label=="hit");CHECK(Is(e,"x",3));
    // Index dimensions must not freeze mutable formType/formId at the original values.
    Rules(Json::array({Rule({{"formType","Weapon"}},{{"formType",27}}),Rule({{"formType","Book"}},{{"formId",456}}),Rule({{"formId",456}},{{"iconLabel","hit"}})}));e=Fields({{"formType",41},{"formId",123}});CHECK(Resolve(e).label=="hit");
    Rules(Json::array({Rule({{"formId",123}},{{"formId",456}}),Rule({{"formId",456}},{{"iconLabel","hit"}})}));e=Fields({{"formId",123}});CHECK(Resolve(e).label=="hit");
    // No non-icon assignment: preserve existing default icon/color.
    Rules(Json::array({Rule({{"x",1}},{{"iconExtra",true}})}));e=Fields({{"x",1},{"iconLabel","existing"},{"iconColor",5}});icon=Resolve(e);CHECK(icon.label=="existing");CHECK(icon.color==5);CHECK(icon.origin==IconOrigin::Default);
    Load({});e={};CHECK(Resolve(e).origin==IconOrigin::None);
}
void Defaults() {
    Load({});
    auto icon=[](int form,int sub=-1,int weight=-1,int status=-1){auto e=Fields({{"formType",form}});if(sub!=-1)e.Set("subType",sub);if(weight!=-1)e.Set("weightClass",weight);if(status!=-1)e.Set("status",status);return Resolve(e);};
    const char* weapons[]={"default_weapon","weapon_sword","weapon_dagger","weapon_waraxe","weapon_mace","weapon_greatsword","weapon_battleaxe","weapon_hammer","weapon_bow","weapon_crossbow","weapon_staff","weapon_pickaxe","weapon_woodaxe"};
    for(int i=0;i<13;++i){CHECK(icon(41,i).label==weapons[i]);CHECK(icon(41,i).color==0xA4A5BF);}
    CHECK(icon(41,99).label=="default_weapon");CHECK(icon(42,0).label=="weapon_arrow");CHECK(icon(42,1).label=="weapon_bolt");
    for(int w:{0,1,2,3,4,99})for(int s=0;s<14;++s){auto i=icon(26,s,w);CHECK(i.origin==IconOrigin::Default);CHECK(i.color==(w==0?0x756000:w==1?0x6B7585:0xEDDA87));}
    CHECK(icon(26,9,0).label=="armor_circlet");CHECK(icon(26,10,4).label=="armor_amulet");CHECK(icon(26,12,1).label=="armor_ring");CHECK(icon(26,11,4).label=="default_armor");CHECK(icon(26,10,3).label=="default_armor");
    CHECK(icon(27,0).label=="book_tome");CHECK(icon(27,1).label=="book_note");CHECK(icon(27,2).label=="book_note");CHECK(icon(27).label=="default_book");
    CHECK(icon(30).label=="default_ingredient");CHECK(icon(31).label=="misc_torch");CHECK(icon(45).label=="default_key");CHECK(icon(119).label=="default_shout");
    const char* potions[]={"potion_health","potion_health","potion_health","potion_magic","potion_magic","potion_magic","potion_stam","potion_stam","potion_stam","potion_fire","potion_shock","potion_frost","default_potion","food_wine","default_food","potion_poison"};
    for(int i=0;i<16;++i)CHECK(icon(46,i).label==potions[i]);
    const char* states[]={"empty","partial","full"};
    for(int size=1;size<=5;++size)for(int s=0;s<3;++s)CHECK(icon(52,size,-1,s).label==std::string(size>=4?"soulgem_grand":"soulgem_")+states[s]);
    CHECK(icon(52,6).label=="soulgem_azura");CHECK(icon(52).label=="default_soulgem");
    const char* misc[]={"misc_gem","misc_dragonclaw","misc_artifact","misc_leather","misc_strips","misc_hide","misc_remains","misc_ingot","default_misc","clothing_body","default_misc","misc_wood","default_misc","default_misc","default_misc","default_misc","default_misc","default_misc","default_misc","misc_clutter","misc_lockpick","misc_gold"};
    for(int i=0;i<22;++i)CHECK(icon(32,i).label==misc[i]);
    for(int school=18;school<=22;++school){auto e=Fields({{"formType",22},{"school",school}});CHECK(Resolve(e).origin==IconOrigin::Default);}
    for(int type:{22,23})for(int r:{41,42,43}){
        auto e=Fields({{"formType",type},{"school",20},{"resistance",r}});auto i=Resolve(e);
        CHECK(i.label==(r==41?"magic_fire":r==42?"magic_shock":"magic_frost"));
        CHECK(i.color==(r==41?0xC73636:r==42?(type==23?0xFFFF00:0xEAAB00):0x1FFBFF));
    }
    // Read-only immutable ruleset shared by independent entries.
    std::atomic<bool> ok=true;
    auto run=[&]{for(int i=0;i<100;++i){auto e=Fields({{"formType",41},{"subType",1}});if(Resolve(e).label!="weapon_sword")ok=false;}};
    std::thread a(run),b(run);a.join();b.join();CHECK(ok.load());
}
void IndexedDisjunctions() {
    // PropertyParser.cpp anyOf: distinct and duplicate IDs must preserve global ordering.
    Rules(Json::array({
        Rule({{"formId",{{"anyOf",Json::array({1,2,2,{{"anyOf",Json::array({3,4})}}})}}}},{{"iconLabel","id"}}),
        Rule({{"formType","Book"}},{{"iconLabel","hit"}})
    }));
    for(int id:{1,2,3,4}) { CHECK(Hit(Fields({{"formId",id},{"formType",27}}))); CHECK(!Hit(Fields({{"formId",id}}))); }
    Entry e=Fields({{"formId",Value::Array{1,4}},{"formType",27}});CHECK(Resolve(e).label=="hit");
    // A numeric disjunction containing null/range is not an exact-key index.
    Rules(Json::array({Rule({{"formId",{{"anyOf",Json::array({1,nullptr})}}}})}));CHECK(Hit({}));
    Rules(Json::array({Rule({{"formId",{{"anyOf",Json::array({1,{{"min",5}}})}}}})}));CHECK(Hit(Fields({{"formId",6}})));
    // RuleParser.cpp:26 calls asString before property dispatch: container formType is invalid.
    CHECK(Rules(Json::array({Rule({{"formType",{{"anyOf",Json::array({"Book","Armor"})}}}})})).rejected==1);
    // Keyword prefilter must be disabled if info assignments can replace that field.
    Rules(Json::array({Rule({{"x",1}},{{"keywords","changed"}}),Rule({{"keywords","original"}},{{"iconLabel","hit"}})}));
    CHECK(!Hit(Fields({{"x",1},{"keywords",Value::Keywords{"original"}}})));
    // Numeric -1 is still a number to IconUtil; it follows the clothing default branch.
    Load({});e=Fields({{"formType",26},{"weightClass",-1},{"subType",3}});CHECK(Resolve(e).label=="clothing_body");
}
void Performance() {
    Json rs=Json::array();
    for(int id=1;id<=1200;++id)rs.push_back(Rule({{"formId",id}},{{"iconLabel","specific"}}));
    for(int i=0;i<100;++i)rs.push_back(Rule({{"formType","Armor"},{"keywords","k"+std::to_string(i)}},{{"iconLabel","armor"}}));
    for(int i=0;i<20;++i)rs.push_back(Rule({{"keywords","common"+std::to_string(i)}},{{"iconLabel","common"}}));
    CHECK(Rules(rs).rules==1320);
    for(int count:{100,500}) {
        std::vector<Entry> original;
        for(int i=1;i<=count;++i){RawFields r;r.formType=i%2?41:26;r.formId=i;r.weaponType=1;r.armorType=0;r.partMask=4;r.keywords=Value::Keywords{"other"};original.push_back(Derive(r));}
        double maximum=0,total=0;std::size_t checksum=0;
        for(int pass=0;pass<21;++pass){auto entries=original;auto start=std::chrono::steady_clock::now();for(auto& e:entries)checksum+=Resolve(e).label.size();auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(pass){maximum=std::max(maximum,ms);total+=ms;}}
        std::cout<<"AC-7 rules=1320 entries="<<count<<" mean_ms="<<total/20<<" max_ms="<<maximum<<" checksum="<<checksum<<'\n';
        CHECK(maximum<(count==100?1.0:5.0));CHECK(checksum==static_cast<std::size_t>(count)*21*8);
    }
}
}
int main(){Matching();EnumsAndIds();Parts();ParsingAndValidation();StagesAndOrder();Defaults();IndexedDisjunctions();Performance();std::cout<<"I4RulesTests: "<<checks<<" checks passed\n";}

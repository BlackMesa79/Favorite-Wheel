#include "ItemInfo.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
void Check(bool value,const char* what){if(!value){std::cerr<<what<<'\n';std::exit(1);}}
int main() {
    using namespace Wheel;
    const auto nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    Check(!ValidStat(nan) && !ValidStat(inf) && !ValidStat(-1) && ValidStat(0).has_value(),"Bad engine stats omitted; zero is meaningful");
    Check(InfoNumber(12)=="12" && InfoNumber(12.25f)=="12.2" && InfoNumber(-.01f)=="0","Stable numeric display and signed zero");
    ItemInfo info;info.weight=0.f;info.value=0;info.damage=nan;info.magicka=8.5f;info.costPerSecond=true;
    AddEffect(info,"Secret",50,5,2,true,false,false,false);
    AddEffect(info,"Cure",nan,30,5,false,true,true,true);
    AddEffect(info,"Restore",50,0,0,false,false,false,false);
    AddEffect(info,"Drain",-3,12,0,false,false,false,false);
    for(int i=0;i<8;++i)AddEffect(info,"Other",10,20,5,false,false,false,false);
    Check(info.effects.size()==6 && info.moreEffects==5 && info.effects[0].name=="Cure","Hidden effects excluded and visible overflow counted");
    Check(!info.effects[0].magnitude && !info.effects[0].duration && !info.effects[0].area,"No-value UI flags respected");
    auto tr=[](const char* key){return std::string(key)=="infoPerSecond"?std::string("/秒"):std::string(key);};
    const auto text=DescribeItem(info,tr);
    Check(text.stats.size()==3 && text.stats[0].value=="8.5/秒" && text.stats[1].value=="0" && text.stats[2].value=="0","Invalid values omitted; concentration and zero stats formatted");
    Check(text.effects[0].value.empty() && text.effects[1].value=="infoMagnitude 50" && text.effects[2].value=="infoMagnitude -3 · 12 infoSeconds","Instant/negative effects are not misrepresented as duration or percentages");
    Check(text.heading=="infoEffects" && text.more=="+5 infoMoreEffects","Base effect and overflow captions");
    info.enchantment=true;Check(DescribeItem(info,tr).heading=="infoEnchantment","Enchantment distinguished from consumable/spell effects");
    ItemInfo unicode;std::string longName;for(int i=0;i<100;++i)longName+="甲";
    AddEffect(unicode,longName,0,0,0,false,false,false,false);
    Check(unicode.effects[0].name==longName.substr(0,255)+"...","Long mod effect name remains UTF-8 aligned");
    Check(ItemInfoGlyphs(unicode).find("甲")!=std::string::npos,"Effect names available for font prewarming");
    ItemInfo unnamed;AddEffect(unnamed,"",inf,0,0,false,false,false,false);
    Check(DescribeItem(unnamed,tr).effects[0].label=="infoUnnamedEffect" && DescribeItem(unnamed,tr).effects[0].value.empty(),"Empty/nonfinite effect has safe fallback");
    Check(!ItemInfo{}.HasData() && DescribeItem(ItemInfo{},tr).stats.empty(),"No data leaves existing function card usable");
    std::cout<<"Item info finite values, visibility flags, units, UTF-8, localization and overflow passed\n";
}

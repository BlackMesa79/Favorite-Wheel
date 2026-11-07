#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
namespace Wheel::Ordering {
    enum class Mode { Default, NameAscending, NameDescending, EquippedFirst, Custom, Type, Slot, StatDescending, StatAscending, WeightAscending, ValueDescending };
    inline bool Numeric(Mode mode){return mode>=Mode::StatDescending && mode<=Mode::ValueDescending;}
    inline Mode NextMode(Mode current,bool typed,bool slots,bool numbers=false,bool physical=false) {
        do {current=static_cast<Mode>((static_cast<int>(current)+1)%11);}
        while((current==Mode::Type && !typed) || (current==Mode::Slot && !slots) || ((current==Mode::StatAscending || current==Mode::StatDescending) && !numbers) || ((current==Mode::WeightAscending || current==Mode::ValueDescending) && !physical));
        return current;
    }
    struct Key {
        std::uint32_t form=0,enchantment=0,health=0;
        std::uint16_t unique=0;
        std::string name;
        bool operator==(const Key&)const=default;
    };
    struct Group {Mode mode=Mode::Default;std::vector<Key> order;};
    constexpr int groupCount=20;
    using State=std::array<Group,groupCount>;
    constexpr std::size_t maxKeys=10000;
    inline std::string Token(const Key& k) {
        return std::to_string(k.form)+":"+std::to_string(k.enchantment)+":"+std::to_string(k.health)+":"+std::to_string(k.unique)+":"+k.name;
    }
    inline void Observe(Group& group,const std::vector<Key>& keys) {
        std::unordered_map<std::string,bool> known;
        for(const auto& key:group.order)known.emplace(Token(key),true);
        for(const auto& key:keys)if(group.order.size()<maxKeys && known.emplace(Token(key),true).second)group.order.push_back(key);
    }
    inline void RemoveDuplicateKeys(State& state) {
        for(auto& group:state) {
            std::unordered_map<std::string,bool> seen;
            std::erase_if(group.order,[&](const Key& key){return !seen.emplace(Token(key),true).second;});
        }
    }
    template<class It,class Identify> void Sort(Group& group,It begin,It end,Identify identity) {
        if(group.mode==Mode::Default)return;
        std::unordered_map<std::string,std::size_t> ranks;
        if(group.mode==Mode::Custom) {
            std::vector<Key> keys;for(auto it=begin;it!=end;++it)keys.push_back(identity(*it));Observe(group,keys);
            for(std::size_t i=0;i<group.order.size();++i)ranks.emplace(Token(group.order[i]),i);
        }
        auto rank=[&](const auto& x){auto it=ranks.find(Token(identity(x)));return it==ranks.end()?maxKeys:it->second;};
        std::stable_sort(begin,end,[&](const auto& a,const auto& b){
            if(group.mode==Mode::NameAscending)return a.name<b.name;
            if(group.mode==Mode::NameDescending)return a.name>b.name;
            if(group.mode==Mode::EquippedFirst)return a.equipped>b.equipped;
            if(group.mode==Mode::Type || group.mode==Mode::Slot) {
                const int av=group.mode==Mode::Type?a.sortType:a.sortSlot;
                const int bv=group.mode==Mode::Type?b.sortType:b.sortSlot;
                return av!=bv?av<bv:a.name<b.name;
            }
            if(Numeric(group.mode)) {
                auto valid=[](auto x){return x && std::isfinite(*x) && *x>=0;};
                const bool av=valid(a.sortNumber),bv=valid(b.sortNumber);
                if(av!=bv)return av; // Unknown is last in both directions.
                if(av && *a.sortNumber!=*b.sortNumber) {
                    const bool ascending=group.mode==Mode::StatAscending || group.mode==Mode::WeightAscending;
                    return ascending?*a.sortNumber<*b.sortNumber:*a.sortNumber>*b.sortNumber;
                }
                return a.name<b.name;
            }
            return rank(a)<rank(b);
        });
    }
    inline bool Move(Group& group,const Key& selected,const Key& neighbor) {
        auto a=std::find(group.order.begin(),group.order.end(),selected),b=std::find(group.order.begin(),group.order.end(),neighbor);
        if(a==group.order.end() || b==group.order.end() || a==b)return false;
        std::iter_swap(a,b);return true;
    }
    inline bool InsertAt(Group& group,const Key& selected,const Key& target) {
        auto a=std::find(group.order.begin(),group.order.end(),selected),b=std::find(group.order.begin(),group.order.end(),target);
        if(a==group.order.end() || b==group.order.end() || a==b)return false;
        if(a<b)std::rotate(a,a+1,b+1);
        else std::rotate(b,a,a+1);
        return true;
    }
    inline std::string Encode(const State& state) {
        std::ostringstream out;out.imbue(std::locale::classic());out<<"FWS 2\n";
        for(const auto& g:state) {
            out<<static_cast<int>(g.mode)<<' '<<g.order.size()<<'\n';
            for(const auto& k:g.order)out<<k.form<<' '<<k.enchantment<<' '<<k.health<<' '<<k.unique<<' '<<std::quoted(k.name)<<'\n';
        }return out.str();
    }
    inline bool Decode(const std::string& text,State& state) {
        if(text.size()>16*1024*1024)return false;
        std::istringstream in(text);in.imbue(std::locale::classic());std::string magic;int version;
        if(!(in>>magic>>version) || magic!="FWS" || (version!=1 && version!=2))return false;
        State next;
        for(int index=0;index<(version==1?18:groupCount);++index) {
            auto& g=next[index];
            int mode;std::size_t n;
            if(!(in>>mode>>n) || mode<0 || mode>10 || n>maxKeys)return false;
            g.mode=static_cast<Mode>(mode);std::unordered_map<std::string,bool> seen;
            for(std::size_t i=0;i<n;++i) {
                Key k;unsigned uid;
                if(!(in>>k.form>>k.enchantment>>k.health>>uid>>std::quoted(k.name)) || !k.form || uid>65535 || k.name.size()>4096)return false;
                k.unique=static_cast<std::uint16_t>(uid);
                if(!seen.emplace(Token(k),true).second)return false;
                g.order.push_back(std::move(k));
            }
        }
        in>>std::ws;if(!in.eof())return false;state=std::move(next);return true;
    }
}

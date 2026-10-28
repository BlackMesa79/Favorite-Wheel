#pragma once
#include "InputBindings.h"
#include "OutfitModel.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Wheel::Cycles {
    inline constexpr int groupCount=8,memberLimit=64;
    struct Group {
        std::uint32_t id=0;
        std::string name;
        int key=-1,modifiers=0;
        std::vector<std::uint32_t> members;
        std::uint32_t current=0;
        bool operator==(const Group&)const=default;
    };
    using Groups=std::array<Group,groupCount>;
    inline Groups Defaults() {
        Groups groups;
        for(int i=0;i<groupCount;++i){groups[i].id=i+1;groups[i].key=i+2;groups[i].modifiers=1;}
        return groups;
    }
    inline bool ValidChord(int key,int modifiers) {
        if(key==-1)return modifiers==0;
        return key>=2 && key<255 && key!=15 && key!=28 && key!=156 && key!=60 &&
            key!=219 && key!=220 && key!=221 && !ModifierBit(key) &&
            modifiers>=0 && modifiers<=7 && !(modifiers==0 && key>=2 && key<=9);
    }
    inline bool EntranceConflict(int key,int modifiers,int favorite,int favoriteModifier,int action,int actionModifier,int switchKey) {
        return key>=0 && (KeyboardOpening(key,modifiers,favorite,favoriteModifier,action,actionModifier)!=Opening::None ||
            (modifiers==0 && key==switchKey));
    }
    inline bool Valid(const Groups& groups) {
        for(int i=0;i<groupCount;++i) {
            const auto& g=groups[i];
            if(g.id!=static_cast<unsigned>(i+1) || (!g.name.empty() && !Outfits::ValidName(g.name)) ||
                !ValidChord(g.key,g.modifiers) || g.members.size()>memberLimit)return false;
            for(int j=0;j<i;++j)if(g.key>=0 && groups[j].key==g.key && groups[j].modifiers==g.modifiers)return false;
            for(std::size_t at=0;at<g.members.size();++at)
                if(!g.members[at] || g.members[at]>1000000 || std::find(g.members.begin(),g.members.begin()+at,g.members[at])!=g.members.begin()+at)return false;
            if(g.current && std::find(g.members.begin(),g.members.end(),g.current)==g.members.end())return false;
        }
        return true;
    }
    inline std::string Encode(const Groups& groups) {
        std::ostringstream out;out<<"FWCY 1\n";
        for(const auto& g:groups) {
            out<<g.id<<' '<<std::quoted(g.name)<<' '<<g.key<<' '<<g.modifiers<<' '<<g.current<<' '<<g.members.size();
            for(auto id:g.members)out<<' '<<id;
            out<<'\n';
        }
        return out.str();
    }
    inline bool Decode(const std::string& text,Groups& result,std::uint32_t version=1) {
        if(version!=1 || text.size()>65536)return false;
        std::istringstream in(text);std::string magic;int format=0;Groups loaded;
        if(!(in>>magic>>format) || magic!="FWCY" || format!=1)return false;
        for(auto& g:loaded) {
            int count=0;
            if(!(in>>g.id>>std::quoted(g.name)>>g.key>>g.modifiers>>g.current>>count) || count<0 || count>memberLimit)return false;
            for(int i=0;i<count;++i){std::uint32_t id=0;if(!(in>>id))return false;g.members.push_back(id);}
        }
        in>>std::ws;if(!in.eof() || !Valid(loaded))return false;
        result=std::move(loaded);return true;
    }
    inline std::uint32_t Match(const Groups& groups,int key,int modifiers,bool insideWheel) {
        if(key<0)return 0;
        for(const auto& g:groups)if(g.key==key && g.modifiers==modifiers && (insideWheel || !g.members.empty()))return g.id;
        return 0;
    }
    enum class AppendResult {Added,Duplicate,Full,Invalid};
    inline AppendResult Append(Group& g,std::uint32_t preset) {
        if(!preset || preset>1000000)return AppendResult::Invalid;
        if(std::find(g.members.begin(),g.members.end(),preset)!=g.members.end())return AppendResult::Duplicate;
        if(g.members.size()>=memberLimit)return AppendResult::Full;
        g.members.push_back(preset);return AppendResult::Added;
    }
    inline bool Remove(Group& g,std::uint32_t preset) {
        const auto removed=std::erase(g.members,preset)>0;if(g.current==preset)g.current=0;return removed;
    }
    inline bool Move(Group& g,std::uint32_t preset,int direction) {
        const auto it=std::find(g.members.begin(),g.members.end(),preset);
        if(it==g.members.end())return false;
        const auto index=std::distance(g.members.begin(),it),next=index+(direction<0?-1:1);
        if(next<0 || next>=static_cast<std::ptrdiff_t>(g.members.size()))return false;
        std::swap(g.members[index],g.members[next]);return true;
    }
    inline std::vector<std::uint32_t> Ordered(const Group& g) {
        std::vector<std::uint32_t> result;
        auto it=std::find(g.members.begin(),g.members.end(),g.current);
        const auto start=it==g.members.end()?0:(std::distance(g.members.begin(),it)+1)%g.members.size();
        for(std::size_t i=0;i<g.members.size();++i)result.push_back(g.members[(start+i)%g.members.size()]);
        return result;
    }
    inline bool Commit(Group& g,std::uint32_t preset,bool success) {
        if(!success || std::find(g.members.begin(),g.members.end(),preset)==g.members.end())return false;
        g.current=preset;return true;
    }
}

#include "CombatCycles.h"
#include "CombatCycleModel.h"
#include "CombatPresets.h"
#include "Settings.h"
#include "UIResources.h"
#include <mutex>

namespace Wheel::Cycles {
    namespace {
        std::mutex mutex;
        Groups groups=Defaults();
        std::uint64_t generation=1;
        Group* Find(std::uint32_t id){return id>=1 && id<=groupCount?&groups[id-1]:nullptr;}
        void Tell(const char* key){RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),key).c_str());}
        std::string Name(const Group& g){return g.name.empty()?Tr(Config(),"cycleDefault")+" "+std::to_string(g.id):g.name;}
        std::string Chord(const Group& g) {
            if(g.key<0)return Tr(Config(),"modifierNone");
            auto config=Config();config.hotkey=g.key;
            return (g.modifiers?ModifierLabel(config,g.modifiers)+" + ":"")+KeyLabel(config);
        }
        std::string Position(const Group& g) {
            const auto it=std::find(g.members.begin(),g.members.end(),g.current);
            return std::to_string(it==g.members.end()?0:std::distance(g.members.begin(),it)+1)+"/"+std::to_string(g.members.size());
        }
    }
    std::string Binding(std::uint32_t id){std::lock_guard lock(mutex);auto g=Find(id);return g?Chord(*g):"";}
    void AnnotateMembership(std::vector<Item>& items) {
        const auto label=Tr(Config(),"cycleDefault");
        std::lock_guard lock(mutex);
        for(auto& item:items) {
            item.centreNote.clear();
            if(item.action!=ActionKind::Combat && item.action!=ActionKind::CycleMember)continue;
            const auto preset=item.action==ActionKind::Combat?item.actionId:item.actionIndex;
            std::string ids;
            for(const auto& group:groups)
                if(std::find(group.members.begin(),group.members.end(),preset)!=group.members.end()) {
                    if(!ids.empty())ids+=" / ";
                    ids+=std::to_string(group.id);
                }
            if(!ids.empty())item.centreNote=label+"\n"+ids;
        }
    }
    std::string Names(){std::lock_guard lock(mutex);std::string text;for(const auto& g:groups)text+=Name(g)+Chord(g);return text;}
    std::uint32_t Match(int key,int modifiers,bool insideWheel){std::lock_guard lock(mutex);return Match(groups,key,modifiers,insideWheel);}
    std::vector<Item> Entries(std::uint32_t id) {
        Groups copies;{std::lock_guard lock(mutex);copies=groups;}
        auto presets=Combat::Entries();std::vector<Item> entries;
        if(id>=1 && id<=groupCount) {
            Item back;back.name=Tr(Config(),"cycleTitle");back.action=ActionKind::FunctionBack;back.magic=true;entries.push_back(back);
            Item capture;capture.name=Tr(Config(),"cycleCapture");capture.action=ActionKind::CycleCapture;capture.actionId=id;capture.magic=true;entries.push_back(capture);
            for(auto preset:copies[id-1].members) {
                auto it=std::find_if(presets.begin(),presets.end(),[&](const Item& p){return p.action==ActionKind::Combat && p.actionId==preset;});
                Item item;if(it!=presets.end())item=*it;
                else {item.name=Tr(Config(),"combatUnavailable")+" #"+std::to_string(preset);item.usable=false;}
                item.action=ActionKind::CycleMember;item.actionId=id;item.actionIndex=preset;item.quickSlot=-1;
                item.detail+=(item.detail.empty()?"":"\n")+Tr(Config(),"cycleMemberHint");entries.push_back(std::move(item));
            }
            return entries;
        }
        for(const auto& g:copies) {
            Item item;item.name=Name(g);item.magic=true;item.action=ActionKind::Cycle;item.actionId=g.id;item.count=static_cast<int>(g.members.size());
            item.usable=!g.members.empty();
            item.detail=Chord(g)+" · "+Position(g)+"\n"+Tr(Config(),"cycleUse")+"\n"+Tr(Config(),"cycleStorage");
            for(const auto& p:presets)if(p.action==ActionKind::Combat && p.actionId==g.current)item.equipped=p.equipped;
            entries.push_back(std::move(item));
        }
        return entries;
    }
    bool Append(std::uint32_t id,std::uint32_t preset) {
        if(Combat::Busy())return false;
        {std::lock_guard lock(mutex);auto g=Find(id);if(!g)return false;if(g->members.size()>=memberLimit){Tell("cycleFull");return false;}}
        if(!preset)preset=Combat::CaptureForCycle();
        if(!preset || !Combat::Exists(preset))return false;
        std::lock_guard lock(mutex);auto g=Find(id);if(!g)return false;
        const auto result=Append(*g,preset);
        if(result==AppendResult::Duplicate){Tell("cycleDuplicate");return true;}
        if(result!=AppendResult::Added){Tell("cycleFull");return false;}
        RE::SendHUDMessage::ShowHUDMessage((Tr(Config(),"cycleAdded")+": "+Name(*g)+" ("+std::to_string(g->members.size())+")").c_str());
        SKSE::log::info("Cycle appended: group={} preset={} members={}",id,preset,g->members.size());return true;
    }
    bool Rename(std::uint32_t id,const std::string& name) {
        if(Combat::Busy() || !Outfits::ValidName(name))return false;
        std::lock_guard lock(mutex);auto g=Find(id);if(!g)return false;g->name=name;return true;
    }
    bool Bind(std::uint32_t id,int key,int modifiers) {
        if(Combat::Busy() || !ValidChord(key,modifiers)){Tell("cycleInvalidKey");return false;}
        std::lock_guard lock(mutex);auto g=Find(id);if(!g)return false;
        for(const auto& other:groups)if(other.id!=id && key>=0 && other.key==key && other.modifiers==modifiers){Tell("cycleConflict");return false;}
        g->key=key;g->modifiers=modifiers;Tell("cycleBound");return true;
    }
    bool Clear(std::uint32_t id) {
        if(Combat::Busy())return false;
        std::lock_guard lock(mutex);auto g=Find(id);if(!g)return false;g->members.clear();g->current=0;return true;
    }
    bool Remove(std::uint32_t id,std::uint32_t preset) {
        if(Combat::Busy())return false;
        std::lock_guard lock(mutex);auto g=Find(id);return g && Remove(*g,preset);
    }
    bool Move(std::uint32_t id,std::uint32_t preset,int direction) {
        if(Combat::Busy())return false;
        std::lock_guard lock(mutex);auto g=Find(id);return g && Move(*g,preset,direction);
    }
    void PresetRemoved(std::uint32_t preset){std::lock_guard lock(mutex);for(auto& g:groups)Remove(g,preset);}
    void Apply(std::uint32_t id,std::uint32_t specific) {
        if(Combat::Busy())return;
        Group copy;std::uint64_t stamp;
        {std::lock_guard lock(mutex);auto g=Find(id);if(!g)return;copy=*g;stamp=generation;}
        if(copy.members.empty()){Tell("cycleEmpty");return;}
        if(specific && std::find(copy.members.begin(),copy.members.end(),specific)==copy.members.end())return;
        const auto next=Combat::FirstAvailable(specific?std::vector<std::uint32_t>{specific}:Ordered(copy));
        if(!next){Tell("cycleUnavailable");return;}
        Combat::Apply(next,[id,next,stamp](bool success) {
            std::lock_guard lock(mutex);auto g=Find(id);
            if(stamp!=generation || !g || !Commit(*g,next,success))return;
            RE::SendHUDMessage::ShowHUDMessage((Name(*g)+": "+Position(*g)).c_str());
            SKSE::log::info("Cycle committed: group={} preset={} position={}",id,next,Position(*g));
        });
    }
    void Save(SKSE::SerializationInterface* api) {
        std::lock_guard lock(mutex);const auto text=Encode(groups);
        if(!api->WriteRecord(record,1,text.data(),static_cast<std::uint32_t>(text.size())))SKSE::log::error("Unable to save combat cycles");
    }
    void Revert(){std::lock_guard lock(mutex);groups=Defaults();++generation;}
    void Load(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length) {
        if(version!=1 || length>65536){SKSE::log::warn("Unsupported cycle record");return;}
        std::string text(length,'\0');Groups loaded;
        if(api->ReadRecordData(text.data(),length)!=length || !Decode(text,loaded,version)){SKSE::log::error("Invalid cycle record");return;}
        std::lock_guard lock(mutex);groups=std::move(loaded);++generation;
    }
    void FinalizeLoad() {
        Groups copy;{std::lock_guard lock(mutex);copy=groups;}
        for(auto& g:copy)for(auto id:std::vector<std::uint32_t>(g.members))if(!Combat::Exists(id))Remove(g,id);
        std::lock_guard lock(mutex);groups=std::move(copy);
    }
}

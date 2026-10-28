#include "InventoryOrder.h"
#include <bit>
#include <mutex>
namespace Wheel::Ordering {
    namespace {
        std::mutex mutex;
        State state;
        int Index(bool all,Category category){return (all?9:0)+static_cast<int>(category);}
        Key Identity(const Item& item) {
            // Never serialize native ExtraDataList addresses or quick-slot assignments.
            return {item.key.form,item.key.enchantment,std::bit_cast<std::uint32_t>(item.key.health),
                static_cast<std::uint16_t>(item.weaponStack || item.miscDisplayStack?0:item.key.unique),item.name};
        }
    }
    Mode Current(bool all,Category category){std::lock_guard lock(mutex);return state[Index(all,category)].mode;}
    void Cycle(bool all,Category category){std::lock_guard lock(mutex);auto& g=state[Index(all,category)];g.mode=NextMode(g.mode,category==Category::Weapons || category==Category::Armor || category==Category::Spells,category==Category::Armor,category==Category::Weapons || category==Category::Armor || category==Category::Spells,category==Category::Weapons || category==Category::Armor);}
    void SetCustom(bool all,Category category,const std::vector<Item>& items) {
        std::lock_guard lock(mutex);auto& g=state[Index(all,category)];
        if(g.order.empty()){std::vector<Key> keys;for(const auto& item:items)if(item.category==category)keys.push_back(Identity(item));Observe(g,keys);}
        g.mode=Mode::Custom;
    }
    void Apply(std::vector<Item>& items,bool all) {
        std::lock_guard lock(mutex);
        for(int c=0;c<9;++c) {
            auto begin=std::find_if(items.begin(),items.end(),[&](const Item& x){return int(x.category)==c;});
            auto end=std::find_if(begin,items.end(),[&](const Item& x){return int(x.category)!=c;});
            Sort(state[(all?9:0)+c],begin,end,Identity);
        }
    }
    bool MoveItem(bool all,Category category,const Item& a,const Item& b){std::lock_guard lock(mutex);return Move(state[Index(all,category)],Identity(a),Identity(b));}
    bool InsertItem(bool all,Category category,const Item& a,const Item& b){std::lock_guard lock(mutex);return InsertAt(state[Index(all,category)],Identity(a),Identity(b));}
    void Revert(){std::lock_guard lock(mutex);state={};}
    void Save(SKSE::SerializationInterface* api) {
        std::lock_guard lock(mutex);auto text=Encode(state);
        if(text.size()>16*1024*1024 || !api->WriteRecord(record,1,text.data(),static_cast<std::uint32_t>(text.size())))SKSE::log::error("Unable to save inventory order");
    }
    void Load(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length) {
        if(version!=1 || length>16*1024*1024)return;
        std::string text(length,'\0');State loaded;
        if(api->ReadRecordData(text.data(),length)!=length || !Decode(text,loaded)){SKSE::log::warn("Invalid inventory order record");return;}
        for(auto& g:loaded)std::erase_if(g.order,[&](Key& k){
            RE::FormID form=0,enchantment=0;
            if(!api->ResolveFormID(k.form,form) || (k.enchantment && !api->ResolveFormID(k.enchantment,enchantment)))return true;
            k.form=form;k.enchantment=enchantment;return false;
        });
        std::lock_guard lock(mutex);state=std::move(loaded);
    }
}

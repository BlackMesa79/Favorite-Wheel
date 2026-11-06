#include "CombatPresets.h"
#include "CombatCycles.h"
#include "CombatPresetModel.h"
#include "InventoryPolicy.h"
#include "MagicCategories.h"
#include "ActorRuntime.h"
#include "FormRuntime.h"
#include "UIResources.h"
#include "Settings.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>

namespace Wheel::Combat {
    namespace {
        std::mutex mutex;
        std::vector<Preset> presets;
        std::vector<SlotBinding> loadedBindings;
        std::atomic_bool busy=false,cancelRequested=false;
        using Clock=std::chrono::steady_clock;
        struct Candidate {Piece piece;int count;bool right,left;RE::TESBoundObject* object;RE::ExtraDataList* extra;bool quest;};
        struct Hands {Target right,left,voice;bool twoHanded=false;};
        struct Job {
            Preset preset;Step step=Step::ClearRight;bool issued=false;
            Clock::time_point started=Clock::now(),issuedAt{},nextCheck{};
            std::shared_ptr<std::atomic_bool> spellCallback;
            std::function<void(bool)> complete;
        };
        std::optional<Job> job; // Game-thread only; cancellation is atomic.
        void Tell(const char* key){if(Config().showActionNotifications)RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),key).c_str());}
        FormRef Ref(RE::TESForm* form) {
            if(!form)return {};
            if(form->GetFormID()<0xFF000000)if(auto file=form->GetFile(0))return {std::string(file->GetFilename()),form->GetLocalFormID()};
            return {"",form->GetFormID()};
        }
        RE::TESForm* Form(const FormRef& ref) {
            auto data=RE::TESDataHandler::GetSingleton();
            const auto id=ref.plugin.empty()?ref.id:data?data->LookupFormID(ref.id,ref.plugin):0;
            return id?RE::TESForm::LookupByID(id):nullptr;
        }
        bool Both(RE::TESForm* form) {
            if(auto w=form?form->As<RE::TESObjectWEAP>():nullptr)return w->IsTwoHandedSword() || w->IsTwoHandedAxe() || w->IsBow() || w->IsCrossbow();
            if(auto s=form?form->As<RE::SpellItem>():nullptr)return s->IsTwoHanded();
            return false;
        }
        bool LeftOnly(RE::TESBoundObject* object) {
            if(auto armor=object->As<RE::TESObjectARMO>())return armor->IsShield();
            if(auto light=object->As<RE::TESObjectLIGH>())return light->CanBeCarried();
            return false;
        }
        bool Supported(RE::TESBoundObject* object) {
            if(auto weapon=object->As<RE::TESObjectWEAP>())return weapon->GetWeaponType()!=RE::WEAPON_TYPE::kHandToHandMelee;
            return LeftOnly(object);
        }
        Piece Describe(RE::TESForm* form,RE::ExtraDataList* extra=nullptr,bool labels=false) {
            Piece x;x.base=Ref(form);const auto name=form->GetName();x.label=name && *name?name:"?";
            if(extra) {
                if(labels)if(auto object=form->As<RE::TESBoundObject>())if(auto text=extra->GetDisplayName(object);text && *text)x.label=text;
                if(auto e=extra->GetByType<RE::ExtraEnchantment>())x.enchantment=Ref(e->enchantment);
                if(auto h=extra->GetByType<RE::ExtraHealth>())x.health=h->health;
                if(auto u=extra->GetByType<RE::ExtraUniqueID>()){x.unique=u->uniqueID;x.owner=Ref(RE::TESForm::LookupByID(u->baseID));}
                if(auto t=extra->GetByType<RE::ExtraTextDisplayData>();t && t->IsPlayerSet()) {
                    x.customName=t->displayName.c_str();
                    if(t->customNameLength>0 && t->customNameLength<x.customName.size())x.customName.resize(t->customNameLength);
                }
            }
            return x;
        }
        std::vector<Candidate> Inventory(bool labels=false) {
            std::vector<Candidate> result;auto player=RE::PlayerCharacter::GetSingleton();if(!player)return result;
            for(auto& [object,data]:player->GetInventory([](RE::TESBoundObject& object){return Supported(&object);})) {
                if(!object || data.first<=0 || !data.second)continue;
                InventoryBudget budget{data.first};
                auto append=[&](RE::ExtraDataList* extra,int count) {
                    if(count<=0)return;
                    const bool right=extra && extra->HasType<RE::ExtraWorn>(),left=extra && extra->HasType<RE::ExtraWornLeft>();
                    const bool leftOnly=LeftOnly(object);
                    result.push_back({Describe(object,extra,labels),count,!leftOnly && right,leftOnly?(right||left):left,
                        object,extra,data.second->IsQuestObject()});
                };
                if(data.second->extraLists)for(auto extra:*data.second->extraLists)if(extra)append(extra,budget.Take(extra->GetCount()));
                append(nullptr,budget.Spare());
            }
            return result;
        }
        bool ReadHand(RE::PlayerCharacter* player,const std::vector<Candidate>& inventory,bool left,Target& target) {
            auto form=player->GetEquippedObject(left);
            // A shield can remain worn/stowed while a two-handed weapon is
            // active. Do not invent an occupied hand from that biped marker.
            if(!form && left && !Both(player->GetEquippedObject(false)))
                for(const auto& c:inventory)if(c.left && LeftOnly(c.object)){form=c.object;break;}
            if(!form){target={};return true;}
            if(auto weapon=form->As<RE::TESObjectWEAP>();weapon && weapon->GetWeaponType()==RE::WEAPON_TYPE::kHandToHandMelee) {
                target={};return true; // Internal fists are empty, not an inventory target.
            }
            if(auto spell=form->As<RE::SpellItem>()) {
                if(spell->GetSpellType()!=RE::MagicSystem::SpellType::kSpell)return false;
                target={Kind::Spell,Describe(form)};return true;
            }
            for(const auto& c:inventory)if(c.object==form && ((left?c.left:c.right) || (Both(form) && (c.right || c.left)))) {
                target={Kind::Object,c.piece};return true;
            }
            return false; // Do not treat unsupported/bound/inaccessible gear as empty.
        }
        const RE::BGSEquipSlot* VoiceSlot() {
            auto defaults=RE::BGSDefaultObjectManager::GetSingleton();
            return defaults?defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kVoiceEquip):nullptr;
        }
        bool ReadVoice(RE::PlayerCharacter* player,Target& target) {
            auto slot=VoiceSlot();auto form=player && slot?player->GetEquippedObjectInSlot(slot):nullptr;
            if(!form){target={};return true;}
            if(form->As<RE::TESShout>()){target={Kind::Voice,Describe(form)};return true;}
            if(auto spell=form->As<RE::SpellItem>();spell && MagicCategory(false,spell->GetSpellType())==Category::Powers) {
                target={Kind::Voice,Describe(form)};return true;
            }
            return false;
        }
        bool ReadHands(const std::vector<Candidate>& inventory,Hands& hands) {
            auto player=RE::PlayerCharacter::GetSingleton();if(!player)return false;
            if(!ReadVoice(player,hands.voice))return false;
            if(CurrentFormMode()==FormMode::Werewolf){hands.right={Kind::Ignore,{}};hands.left={Kind::Ignore,{}};hands.twoHanded=false;return true;}
            if(!ReadHand(player,inventory,false,hands.right) || !ReadHand(player,inventory,true,hands.left))return false;
            if(hands.right.kind!=Kind::Empty && Both(Form(hands.right.piece.base))) {
                hands.twoHanded=true;hands.left={};
            }else if(hands.left.kind!=Kind::Empty && Both(Form(hands.left.piece.base))) {
                hands.twoHanded=true;hands.right=hands.left;hands.left={};
            }
            return true;
        }
        bool Wearing(const Preset& p,const Hands& hands) {
            return SameTarget(p.right,hands.right) && SameTarget(p.left,hands.left) && SameTarget(p.voice,hands.voice);
        }
        const char* ProblemKey(Problem problem) {
            switch(problem) {
            case Problem::Changed:return "combatChanged";
            case Problem::Ambiguous:return "combatAmbiguous";
            case Problem::NotEnough:return "combatNotEnough";
            default:return "combatMissing";
            }
        }
        std::string Check(const Preset& p,const std::vector<Candidate>& inventory,Resolution& resolved) {
            if(CurrentFormMode()==FormMode::Werewolf &&
                (p.right.kind!=Kind::Ignore || p.left.kind!=Kind::Ignore))return Tr(Config(),"combatUnsupported");
            if(p.twoHanded && (p.right.kind==Kind::Empty ||
                (p.left.kind!=Kind::Empty && p.left.kind!=Kind::Ignore)))return Tr(Config(),"combatUnsupported");
            const auto player=RE::PlayerCharacter::GetSingleton();
            for(const auto* target:{&p.right,&p.left,&p.voice})if(target->kind!=Kind::Empty && target->kind!=Kind::Ignore) {
                const bool left=target==&p.left;auto form=Form(target->piece.base);
                const auto label=(target==&p.voice?Tr(Config(),"combatVoice"):left?Tr(Config(),"combatLeft"):Tr(Config(),"combatRight"))+": "+target->piece.label+" — ";
                if(!form)return label+Tr(Config(),"combatMissing");
                if(target->kind==Kind::Voice) {
                    auto shout=form->As<RE::TESShout>();auto spell=form->As<RE::SpellItem>();
                    const auto category=spell?MagicCategory(false,spell->GetSpellType()):std::nullopt;
                    if((!shout || !player || !player->HasShout(shout)) &&
                        (!spell || !player || !player->HasSpell(spell) || !category || *category!=Category::Powers))
                        return label+Tr(Config(),"combatSpellMissing");
                    if(!VoiceSlot())return label+Tr(Config(),"combatUnsupported");
                }else if(target->kind==Kind::Spell) {
                    auto spell=form->As<RE::SpellItem>();
                    if(!spell || spell->GetSpellType()!=RE::MagicSystem::SpellType::kSpell || !player || !player->HasSpell(spell))
                        return label+Tr(Config(),"combatSpellMissing");
                    if(!spell->GetEquipSlot())return label+Tr(Config(),"combatUnsupported");
                }else {
                    auto object=form->As<RE::TESBoundObject>();
                    if(!object || !Supported(object) || (!left && LeftOnly(object)))return label+Tr(Config(),"combatUnsupported");
                    auto weapon=object->As<RE::TESObjectWEAP>();
                    if(p.twoHanded && (!weapon || !weapon->GetEquipSlot()))return label+Tr(Config(),"combatUnsupported");
                }
                if(Both(form)!=(p.twoHanded && !left))return label+Tr(Config(),"combatChanged");
            }
            resolved=Resolve(p,inventory);
            if(!resolved) {
                const auto& target=resolved.left?p.left:p.right;
                return Tr(Config(),resolved.left?"combatLeft":"combatRight")+": "+target.piece.label+" — "+Tr(Config(),ProblemKey(resolved.problem));
            }
            return {};
        }
        std::string Detail(const Preset& p) {
            auto label=[](const Target& target){return target.kind==Kind::Ignore?Tr(Config(),"combatIgnore"):target.kind==Kind::Empty?Tr(Config(),"combatEmptyHand"):target.piece.label;};
            const auto hands=p.twoHanded?Tr(Config(),"combatBoth")+": "+p.right.piece.label:
                Tr(Config(),"combatLeft")+": "+label(p.left)+"\n"+Tr(Config(),"combatRight")+": "+label(p.right);
            return hands+"\n"+Tr(Config(),"combatVoice")+": "+label(p.voice);
        }
        void Finish(const char* message) {
            if(job)SKSE::log::info("Combat preset {} finished: {}",job->preset.id,message);
            auto complete=job?std::move(job->complete):std::function<void(bool)>{};
            job.reset();busy=false;cancelRequested=false;Tell(message);
            if(complete)complete(std::string_view(message)=="combatApplied");
        }
        const RE::BGSEquipSlot* HandSlot(bool left) {
            auto defaults=RE::BGSDefaultObjectManager::GetSingleton();
            return defaults?defaults->GetObject<RE::BGSEquipSlot>(left?RE::DEFAULT_OBJECT::kLeftHandEquip:RE::DEFAULT_OBJECT::kRightHandEquip):nullptr;
        }
        // Existing native Actor.UnequipSpell supplies correct engine cleanup.
        // No additional script/ESP and no unverified relocation for this path.
        struct SpellDone final:RE::BSScript::IStackCallbackFunctor {
            std::shared_ptr<std::atomic_bool> done;
            explicit SpellDone(std::shared_ptr<std::atomic_bool> value):done(std::move(value)){}
            void operator()(RE::BSScript::Variable)override{*done=true;}
            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&)override{}
        };
        RE::BSTSmartPointer<RE::BSScript::Object> ActorObject(RE::PlayerCharacter* player) {
            // Packing finds or creates/binds the native Actor proxy. Dispatch
            // by handle alone can fail when that proxy has not been created.
            RE::BSScript::Variable actor;
            RE::BSScript::PackValue(&actor,static_cast<RE::Actor*>(player));
            return actor.IsObject()?actor.GetObject():RE::BSTSmartPointer<RE::BSScript::Object>{};
        }
        bool ClearSpell(RE::PlayerCharacter* player,RE::SpellItem* spell,bool left) {
            auto vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if(!vm || !vm->GetObjectHandlePolicy())return false;
            auto actor=ActorObject(player);if(!actor)return false;
            job->spellCallback=std::make_shared<std::atomic_bool>(false);
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new SpellDone(job->spellCallback));
            return vm->DispatchMethodCall(actor,"UnequipSpell",RE::MakeFunctionArguments(static_cast<RE::SpellItem*>(spell),left?0:1),callback);
        }
        void UnequipShout(RE::ActorEquipManager* manager,RE::PlayerCharacter* player,RE::TESShout* shout) {
            using Function=void(*)(RE::ActorEquipManager*,RE::Actor*,RE::TESShout*);
            REL::Relocation<Function> unequip{REL::RelocationID(37948,38904)};
            unequip(manager,player,shout);
        }
    }
    std::vector<Item> Entries() {
        auto inventory=Inventory();Hands hands;const bool known=ReadHands(inventory,hands);
        std::vector<Preset> copies;{std::lock_guard lock(mutex);copies=presets;}
        std::vector<Item> result{{{},Category::Weapons,Tr(Config(),"combatSave"),0,false,true,true,ActionKind::SaveCombat}};
        for(const auto& p:copies) {
            Resolution resolved;const auto problem=Check(p,inventory,resolved);
            Item item;item.name=p.name;item.category=Category::Weapons;item.action=ActionKind::Combat;item.actionId=p.id;
            item.magic=true;item.count=int(p.right.kind!=Kind::Empty && p.right.kind!=Kind::Ignore)+
                int(p.left.kind!=Kind::Empty && p.left.kind!=Kind::Ignore)+int(p.voice.kind!=Kind::Empty && p.voice.kind!=Kind::Ignore);
            item.quickSlot=p.quickSlot;
            item.usable=problem.empty();item.equipped=item.usable && known && Wearing(p,hands);
            item.detail=problem.empty()?Detail(p):problem;result.push_back(std::move(item));
        }
        return result;
    }
        std::string Names() {
        std::lock_guard lock(mutex);std::string text;
        for(const auto& p:presets)text+=p.name+p.right.piece.label+p.left.piece.label+p.voice.piece.label;
        return text;
    }
    bool Capture(const std::string& name,std::uint32_t replace) {
        if(!Outfits::ValidName(name) || Busy())return false;
        auto inventory=Inventory(true);Hands hands;
        if(!ReadHands(inventory,hands)){Tell("combatUnsupported");return false;}
        Preset next;next.name=name;next.right=hands.right;next.left=hands.left;next.voice=hands.voice;next.twoHanded=hands.twoHanded;
        if(CurrentFormMode()==FormMode::Werewolf){next.right={Kind::Ignore,{}};next.left={Kind::Ignore,{}};next.twoHanded=false;}
        Resolution resolved;if(!Check(next,inventory,resolved).empty()){Tell("combatCaptureFailed");return false;}
        std::lock_guard lock(mutex);
        auto found=std::find_if(presets.begin(),presets.end(),[&](const auto& p){return p.id==replace;});
        if(replace && found==presets.end())return false;
        if(!replace && presets.size()>=100){Tell("combatLimit");return false;}
        next.id=replace;
        if(replace)next.quickSlot=found->quickSlot;
        if(!replace){next.id=1;for(const auto& p:presets)next.id=std::max(next.id,p.id+1);}
        auto updated=presets;
        if(replace)updated[std::distance(presets.begin(),found)]=next;else updated.push_back(next);
        std::vector<Preset> validated;if(!Decode(Encode(updated),validated)){Tell("combatCaptureFailed");return false;}
        presets=std::move(updated);Tell("combatSaved");
        SKSE::log::info("Combat preset captured: id={} name='{}' right={} left={} twoHanded={}",next.id,name,
            next.right.piece.base.id,next.left.piece.base.id,next.twoHanded);return true;
    }
    bool Rename(std::uint32_t id,const std::string& name) {
        if(!Outfits::ValidName(name) || Busy())return false;std::lock_guard lock(mutex);
        for(auto& p:presets)if(p.id==id){const auto old=p.name;p.name=name;if(Encode(presets).size()>1024*1024){p.name=old;return false;}return true;}
        return false;
    }
    std::string SlotLabel(std::uint32_t id,Slot slot) {
        Preset preset;{std::lock_guard lock(mutex);for(const auto& item:presets)if(item.id==id){preset=item;break;}}
        const auto& target=slot==Slot::Right?preset.right:slot==Slot::Left?preset.left:preset.voice;
        const char* name=slot==Slot::Right?"combatRight":slot==Slot::Left?"combatLeft":"combatVoice";
        const char* mode=target.kind==Kind::Ignore?"combatIgnore":target.kind==Kind::Empty?"combatUnequip":"combatSet";
        return Tr(Config(),name)+": "+Tr(Config(),mode);
    }
    bool CycleSlot(std::uint32_t id,Slot slot) {
        if(Busy())return false;
        if(CurrentFormMode()==FormMode::Werewolf && slot!=Slot::Voice)return false;
        Hands current;const auto inventory=Inventory();
        if(!ReadHands(inventory,current)){Tell("combatUnsupported");return false;}
        std::lock_guard lock(mutex);
        auto found=std::find_if(presets.begin(),presets.end(),[&](const auto& preset){return preset.id==id;});
        if(found==presets.end())return false;
        auto& target=slot==Slot::Right?found->right:slot==Slot::Left?found->left:found->voice;
        if(target.kind!=Kind::Ignore && target.kind!=Kind::Empty) {
            target={Kind::Ignore,{}};
            if(slot==Slot::Right)found->twoHanded=false;
        }else if(target.kind==Kind::Ignore)target={};
        else {
            const auto& actual=slot==Slot::Right?current.right:slot==Slot::Left?current.left:current.voice;
            if(actual.kind==Kind::Empty)target={Kind::Ignore,{}};
            else {
                target=actual;
                if(slot==Slot::Right)found->twoHanded=current.twoHanded;
            }
        }
        if(!Valid(*found)){Tell("combatBlocked");return false;}
        return true;
    }
    bool Exists(std::uint32_t id) {
        std::lock_guard lock(mutex);return std::any_of(presets.begin(),presets.end(),[&](const auto& p){return p.id==id;});
    }
    std::uint32_t CaptureForCycle() {
        if(Busy())return 0;
        auto inventory=Inventory(true);Hands hands;
        if(!ReadHands(inventory,hands)){Tell("combatUnsupported");return 0;}
        Preset next;next.right=hands.right;next.left=hands.left;next.voice=hands.voice;next.twoHanded=hands.twoHanded;
        if(CurrentFormMode()==FormMode::Werewolf){next.right={Kind::Ignore,{}};next.left={Kind::Ignore,{}};next.twoHanded=false;}
        Resolution resolved;if(!Check(next,inventory,resolved).empty()){Tell("combatCaptureFailed");return 0;}
        std::lock_guard lock(mutex);
        for(const auto& p:presets)if(p.twoHanded==next.twoHanded && SameTarget(p.right,next.right) &&
            SameTarget(p.left,next.left) && SameTarget(p.voice,next.voice))return p.id;
        if(presets.size()>=100){Tell("combatLimit");return 0;}
        next.id=1;for(const auto& p:presets)next.id=std::max(next.id,p.id+1);
        next.name=Tr(Config(),"combatDefault")+" "+std::to_string(next.id);
        auto updated=presets;updated.push_back(next);std::vector<Preset> validated;
        if(!Decode(Encode(updated),validated)){Tell("combatCaptureFailed");return 0;}
        presets=std::move(updated);return next.id;
    }
    std::uint32_t FirstAvailable(const std::vector<std::uint32_t>& ids) {
        std::vector<Preset> copies;{std::lock_guard lock(mutex);copies=presets;}
        const auto inventory=Inventory();
        for(auto id:ids)for(const auto& p:copies)if(p.id==id) {
            Resolution resolved;if(Check(p,inventory,resolved).empty())return id;break;
        }
        return 0;
    }
    bool Remove(std::uint32_t id) {
        {std::lock_guard lock(mutex);if(Busy() || !std::erase_if(presets,[&](const auto& p){return p.id==id;}))return false;}
        Cycles::PresetRemoved(id);return true;
    }
    bool BindQuickSlot(std::uint32_t id,int slot,int* assignment) {
        std::lock_guard lock(mutex);
        if(Busy() || !ValidQuickSlot(slot))return false;
        const auto found=std::find_if(presets.begin(),presets.end(),[&](const auto& p){return p.id==id;});
        if(found==presets.end())return false;
        const int next=AssignedQuickSlot(found->quickSlot,slot);
        if(next>=0 && !ClearFavoriteQuickSlot(slot))return false;
        const bool ok=AssignQuickSlot(presets,id,slot,assignment);
        if(ok)SKSE::log::info("Combat quick slot: preset={} slot={} assignment={}",id,slot+1,next);
        return ok;
    }
    void ClearQuickSlot(int slot,std::uint32_t expectedId) {
        std::lock_guard lock(mutex);
        for(auto& p:presets)if(p.quickSlot==slot && (!expectedId || p.id==expectedId)) {
            p.quickSlot=-1;SKSE::log::info("Combat quick slot released to native item: preset={} slot={}",p.id,slot+1);
        }
    }
    std::uint32_t PresetForQuickSlot(int slot){std::lock_guard lock(mutex);return PresetForQuickSlot(presets,slot);}
    void Save(SKSE::SerializationInterface* api) {
        std::lock_guard lock(mutex);const auto text=Encode(presets);
        if(!api->WriteRecord(record,2,text.data(),static_cast<std::uint32_t>(text.size())))SKSE::log::error("Unable to save combat presets");
        const auto bindings=EncodeBindings(presets);
        if(!api->WriteRecord(bindingRecord,1,bindings.data(),static_cast<std::uint32_t>(bindings.size())))SKSE::log::error("Unable to save combat quick slots");
    }
    void Revert(){std::lock_guard lock(mutex);presets.clear();loadedBindings.clear();job.reset();busy=false;cancelRequested=false;}
    void Load(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length) {
        if((version!=1 && version!=2) || length>1024*1024){SKSE::log::warn("Unsupported combat preset record");return;}
        std::string text(length,'\0');std::vector<Preset> loaded;
        if(api->ReadRecordData(text.data(),length)!=length || !Decode(text,loaded,version)){SKSE::log::error("Invalid combat preset record");return;}
        for(auto& p:loaded)for(auto* target:{&p.right,&p.left})for(auto* r:{&target->piece.base,&target->piece.enchantment,&target->piece.owner})
            if(r->plugin.empty() && r->id){RE::FormID resolved=0;if(api->ResolveFormID(r->id,resolved))r->id=resolved;else r->plugin="<unresolved>";}
        std::lock_guard lock(mutex);presets=std::move(loaded);ApplyBindings(presets,loadedBindings);SKSE::log::info("Loaded {} combat presets from character co-save",presets.size());
    }
    void LoadBindings(SKSE::SerializationInterface* api,std::uint32_t version,std::uint32_t length) {
        if(version!=1 || length>1024){SKSE::log::warn("Unsupported combat quick-slot record");return;}
        std::string text(length,'\0');std::vector<SlotBinding> loaded;
        if(api->ReadRecordData(text.data(),length)!=length || !DecodeBindings(text,loaded)) {
            SKSE::log::error("Invalid combat quick-slot record; equipment presets remain available");return;
        }
        std::lock_guard lock(mutex);loadedBindings=std::move(loaded);ApplyBindings(presets,loadedBindings);
        SKSE::log::info("Loaded {} combat quick-slot bindings",loadedBindings.size());
    }
    bool Busy(){return busy.load();}
    void CancelJob(){cancelRequested=true;}
    void Apply(std::uint32_t id,std::function<void(bool)> complete) {
        struct Completion {
            std::function<void(bool)> fn;
            ~Completion(){if(fn)fn(false);}
            void Success(){auto callback=std::move(fn);if(callback)callback(true);}
        } completion{std::move(complete)};
        if(Busy())return;Preset p;
        {std::lock_guard lock(mutex);for(const auto& entry:presets)if(entry.id==id)p=entry;}
        if(!p.id)return;
        const auto inventory=Inventory();Resolution resolved;const auto problem=Check(p,inventory,resolved);
        if(!problem.empty()){if(Config().showActionNotifications)RE::SendHUDMessage::ShowHUDMessage(problem.c_str());SKSE::log::warn("Combat preset {} preflight: {}",id,problem);return;}
        Hands current;if(!ReadHands(inventory,current)){Tell("combatUnsupported");return;}
        if(Wearing(p,current)){Tell("combatApplied");completion.Success();return;}
        auto player=RE::PlayerCharacter::GetSingleton();
        if(!player || ActorRuntime::IsDead(player) ||
            ((p.right.kind!=Kind::Ignore || p.left.kind!=Kind::Ignore) && (!HandSlot(false) || !HandSlot(true))) ||
            (p.voice.kind!=Kind::Ignore && !VoiceSlot())){Tell("combatBlocked");return;}
        for(const auto& c:inventory)if(c.quest && ((p.right.kind!=Kind::Ignore && c.right && !SameTarget(current.right,p.right)) ||
            (p.left.kind!=Kind::Ignore && c.left && (p.twoHanded || !SameTarget(current.left,p.left))))){Tell("combatBlocked");return;}
        Target rawRight,rawLeft;
        if(!ReadHand(player,inventory,false,rawRight) || !ReadHand(player,inventory,true,rawLeft)){Tell("combatBlocked");return;}
        const bool clearSpell=(!p.twoHanded && ((rawRight.kind==Kind::Spell && p.right.kind==Kind::Empty) ||
            (rawLeft.kind==Kind::Spell && p.left.kind==Kind::Empty))) ||
            (current.voice.kind==Kind::Voice && p.voice.kind==Kind::Empty && Form(current.voice.piece.base)->As<RE::SpellItem>());
        auto vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if(clearSpell && (!vm || !vm->GetObjectHandlePolicy() || !ActorObject(player))){Tell("combatBlocked");return;}
        Job next;next.preset=std::move(p);next.complete=std::move(completion.fn);job=std::move(next);cancelRequested=false;busy=true;Tell("combatWorking");
        SKSE::log::info("Combat preset {} apply started",id);
    }
    void Tick(bool valid) {
        if(!job)return;
        if(!valid || cancelRequested.exchange(false)){Finish("combatInterrupted");return;}
        if(Clock::now()-job->started>std::chrono::seconds(15)){Finish("combatPartial");return;}
        if(Clock::now()<job->nextCheck)return;
        job->nextCheck=Clock::now()+std::chrono::milliseconds(30);
        auto player=RE::PlayerCharacter::GetSingleton();auto manager=RE::ActorEquipManager::GetSingleton();
        if(!player || !manager){Finish("combatInterrupted");return;}
        // Never retain inventory/extra pointers across ticks or a mutation.
        auto inventory=Inventory();Resolution resolved;
        const auto problem=Check(job->preset,inventory,resolved);
        if(!problem.empty()) {
            SKSE::log::warn("Combat preset {} stopped at step {}: {}",job->preset.id,static_cast<int>(job->step),problem);
            Finish("combatPartial");return;
        }
        Hands current;if(!ReadHands(inventory,current)){Finish("combatInterrupted");return;}
        if(job->step==Step::Verify) {
            if(Wearing(job->preset,current)){Finish("combatApplied");return;}
            if(!job->issued){job->issued=true;job->issuedAt=Clock::now();}
            else if(Clock::now()-job->issuedAt>=std::chrono::seconds(3))Finish("combatPartial");
            return;
        }
        const bool clearing=job->step==Step::ClearRight || job->step==Step::ClearLeft || job->step==Step::ClearVoice;
        const bool left=job->step==Step::ClearLeft || job->step==Step::EquipLeft;
        const bool voice=job->step==Step::ClearVoice || job->step==Step::EquipVoice;
        const auto& desired=voice?job->preset.voice:left?job->preset.left:job->preset.right;
        Target actual=voice?current.voice:left?current.left:current.right;
        // Clear the actual source independently. A two-handed spell may be
        // reported in both slots or just one; capture normalization must not
        // hide a remaining left spell after the right source was cleared.
        if(clearing && !voice && !ReadHand(player,inventory,left,actual)){Finish("combatInterrupted");return;}
        const bool complete=StepComplete(job->preset,job->step,actual,job->spellCallback && !job->spellCallback->load());
        const auto elapsed=std::chrono::duration<double>(Clock::now()-job->issuedAt).count();
        switch(Decide(complete,job->issued,elapsed)) {
        case Decision::Advance:job->step=static_cast<Step>(static_cast<int>(job->step)+1);job->issued=false;job->spellCallback.reset();return;
        case Decision::Wait:return;
        case Decision::Timeout:
            SKSE::log::warn("Combat preset {} step {} was not observed within 3 seconds",job->preset.id,static_cast<int>(job->step));
            Finish("combatPartial");return;
        case Decision::Issue:break;
        }
        if(clearing && voice) {
            if(actual.kind==Kind::Voice) {
                auto form=Form(actual.piece.base);
                if(auto shout=form?form->As<RE::TESShout>():nullptr)UnequipShout(manager,player,shout);
                else if(auto spell=form?form->As<RE::SpellItem>():nullptr)manager->EquipSpell(player,nullptr,VoiceSlot());
                else {Finish("combatPartial");return;}
            }else {Finish("combatPartial");return;}
        }else if(clearing) {
            if(actual.kind==Kind::Spell) {
                auto form=Form(actual.piece.base);auto spell=form?form->As<RE::SpellItem>():nullptr;
                if(!spell || !ClearSpell(player,spell,left)){Finish("combatPartial");return;}
            }else {
                Preset worn;worn.right=left?Target{}:actual;worn.left=left?actual:Target{};
                const auto match=Resolve(worn,inventory);const int i=match.index[left?1:0];
                if(!match || i<0 || inventory[i].quest){Finish("combatBlocked");return;}
                const auto& c=inventory[i];
                const auto slot=c.object->As<RE::TESObjectARMO>()?nullptr:Both(c.object)?c.object->As<RE::TESObjectWEAP>()->GetEquipSlot():HandSlot(left);
                manager->UnequipObject(player,c.object,c.extra,1,slot,false,false,false,true);
            }
        }else if(voice && desired.kind==Kind::Voice) {
            auto form=Form(desired.piece.base);
            if(auto shout=form?form->As<RE::TESShout>():nullptr)manager->EquipShout(player,shout);
            else if(auto spell=form?form->As<RE::SpellItem>():nullptr)manager->EquipSpell(player,spell,VoiceSlot());
            else {Finish("combatPartial");return;}
        }else if(desired.kind==Kind::Spell) {
            auto spell=Form(desired.piece.base)->As<RE::SpellItem>();
            manager->EquipSpell(player,spell,job->preset.twoHanded?spell->GetEquipSlot():HandSlot(left));
        }else if(desired.kind==Kind::Object) {
            const int i=resolved.index[left?1:0];if(i<0){Finish("combatPartial");return;}
            const auto& c=inventory[i];const auto slot=job->preset.twoHanded?c.object->As<RE::TESObjectWEAP>()->GetEquipSlot():HandSlot(left);
            manager->EquipObject(player,c.object,c.extra,1,slot,false,false,false,true);
        }else {Finish("combatPartial");return;}
        job->issued=true;job->issuedAt=Clock::now();
        SKSE::log::info("Combat preset {} issued step {}",job->preset.id,static_cast<int>(job->step));
    }
}

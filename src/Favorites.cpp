#include "Favorites.h"
#include "IconLibrary.h"
#include "Settings.h"
#include "ActorRuntime.h"
#include "UIResources.h"
#include "EquipPolicy.h"
#include "ItemInfoCapture.h"
#include "InventoryOrder.h"
#include "QuickSlots.h"
#include "InventoryPolicy.h"
#include "WeaponStack.h"
#include "MagicCategories.h"
#include "AlchemyCategories.h"
#include "FormRuntime.h"
#include "MiscDisplayStack.h"
#include <chrono>
#include <unordered_map>
#include <optional>

namespace Wheel {
    namespace {
        bool ImportantProvider() {
            // Inventory collection starts in-game, after all SKSE plugins load.
            // Cache presence once; do not inspect modules per item or per frame.
            static const bool loaded=[] {
                const bool found=GetModuleHandleW(L"DoubleFavoriteAsImportant.dll")!=nullptr;
                if(found)SKSE::log::info("Double Favorite As Important detected: Important items excluded from favorites; protection markers preserved");
                return found;
            }();
            return loaded;
        }
        bool IsFavorite(const RE::ExtraHotkey* hotkey) {
            return InventoryFavorite(hotkey!=nullptr,hotkey?static_cast<int>(hotkey->hotkey.underlying()):-1,ImportantProvider());
        }
        ItemKey Key(RE::TESBoundObject* object, RE::ExtraDataList* extra) {
            ItemKey key{object->GetFormID(), reinterpret_cast<std::uintptr_t>(extra)};
            if (extra) {
                if (const auto uid = extra->GetByType<RE::ExtraUniqueID>()) key.unique = uid->uniqueID;
                if (const auto ench = extra->GetByType<RE::ExtraEnchantment>(); ench && ench->enchantment) key.enchantment = ench->enchantment->GetFormID();
                if (const auto health = extra->GetByType<RE::ExtraHealth>()) key.health = health->health;
            }
            return key;
        }
        Category Classify(RE::TESBoundObject* form) {
            if (form->As<RE::TESObjectWEAP>() || form->As<RE::TESAmmo>()) return Category::Weapons;
            if (form->As<RE::TESObjectARMO>()) return Category::Armor;
            if (const auto potion = form->As<RE::AlchemyItem>()) {
                return AlchemyCategory(potion->IsPoison(),potion->IsFood());
            }
            if (form->As<RE::TESObjectBOOK>()) return Category::Books;
            return Category::Other;
        }
        std::string IconForm(RE::TESForm* form) {
            if(form && form->GetFormID()<0xFF000000)if(auto file=form->GetFile(0))
                return IconLibrary::FormKey(std::string(file->GetFilename()),form->GetLocalFormID());
            return {};
        }
        IconKind ClassifyIcon(RE::TESBoundObject* form) {
            if(auto weapon=form->As<RE::TESObjectWEAP>()) {
                switch(weapon->GetWeaponType()) {
                case RE::WEAPON_TYPE::kOneHandDagger:return IconKind::Dagger;
                case RE::WEAPON_TYPE::kOneHandAxe:case RE::WEAPON_TYPE::kTwoHandAxe:return IconKind::Axe;
                case RE::WEAPON_TYPE::kOneHandMace:return IconKind::Mace;
                case RE::WEAPON_TYPE::kBow:return IconKind::Bow;
                case RE::WEAPON_TYPE::kCrossbow:return IconKind::Crossbow;
                case RE::WEAPON_TYPE::kStaff:return IconKind::Staff;
                default:return IconKind::Sword;
                }
            }
            if(form->As<RE::TESObjectBOOK>())return IconKind::Scroll;
            if(form->As<RE::TESAmmo>())return IconKind::Arrow;
            if(auto armor=form->As<RE::TESObjectARMO>()) {
                using Slot=RE::BGSBipedObjectForm::BipedObjectSlot;
                if(armor->IsShield())return IconKind::Shield;
                if(armor->IsChestpiece())return armor->IsClothing()?IconKind::Robe:IconKind::Armor;
                if(armor->IsHelmet() || armor->IsCirclet())return IconKind::Helmet;
                if(armor->IsGauntlets())return IconKind::Gloves;
                if(armor->IsBoots())return IconKind::Boots;
                if(armor->HasPartOf(Slot::kRing))return IconKind::Ring;
                if(armor->HasPartOf(Slot::kAmulet))return IconKind::Amulet;
                return IconKind::Armor;
            }
            if(auto potion=form->As<RE::AlchemyItem>())return potion->IsPoison()?IconKind::Poison:potion->IsFood()?IconKind::Food:IconKind::Potion;
            if(form->As<RE::TESObjectBOOK>())return IconKind::Book;
            if(form->As<RE::ScrollItem>())return IconKind::Scroll;
            if(form->As<RE::TESObjectLIGH>())return IconKind::Torch;
            return IconKind::Other;
        }
        void SetSortClassification(Item& item,RE::TESBoundObject* object) {
            if(auto weapon=object->As<RE::TESObjectWEAP>())item.sortType=static_cast<int>(weapon->GetWeaponType());
            else if(object->As<RE::TESAmmo>())item.sortType=100;
            else if(auto armor=object->As<RE::TESObjectARMO>()) {
                item.sortType=static_cast<int>(armor->GetArmorType());
                switch(item.icon) {
                case IconKind::Helmet:item.sortSlot=0;break;
                case IconKind::Armor:case IconKind::Robe:item.sortSlot=armor->IsChestpiece()?1:8;break;
                case IconKind::Gloves:item.sortSlot=2;break;
                case IconKind::Boots:item.sortSlot=3;break;
                case IconKind::Shield:item.sortSlot=4;break;
                case IconKind::Ring:item.sortSlot=5;break;
                case IconKind::Amulet:item.sortSlot=6;break;
                default:item.sortSlot=8;break;
                }
            }
        }
        bool TalentInstrument(RE::TESForm* form) {
            if(!form || !form->As<RE::TESObjectMISC>())return false;
            auto data=RE::TESDataHandler::GetSingleton();
            if(!data || !data->LookupModByName("SkyrimsGotTalent-Bards.esp"))return false;
            // Vanilla instruments verified in the 1.76 plugin; exclude its dummy objects.
            switch(form->GetFormID()) {
            case 0xDABA7:case 0xDABA9:case 0xDABAB:case 0x105177:case 0x105109:return true;
            default:return false;
            }
        }
        bool Supported(RE::TESBoundObject* form) {
            if(TalentInstrument(form))return true;
            if (form->As<RE::TESObjectBOOK>() || form->As<RE::TESObjectWEAP>() || form->As<RE::TESAmmo>() || form->As<RE::TESObjectARMO>() || form->As<RE::ScrollItem>() || form->As<RE::AlchemyItem>()) return true;
            if (auto light = form->As<RE::TESObjectLIGH>()) return light->CanBeCarried();
            return false;
        }
        bool PlainInstrument(RE::ExtraDataList* extra) {
            // UniqueIDs do not split ordinary instruments; unknown extras cannot anchor untracked copies.
            for(const auto& data:*extra)switch(data.GetType()) {
            case RE::ExtraDataType::kCount:case RE::ExtraDataType::kUniqueID:break;
            case RE::ExtraDataType::kHotkey:
                if(static_cast<int>(extra->GetByType<RE::ExtraHotkey>()->hotkey.underlying())!=255)return false;
                break;
            default:return false;
            }
            return true;
        }
        bool Plain(RE::ExtraDataList* extra) {
            return !extra->HasType<RE::ExtraEnchantment>() && !extra->HasType<RE::ExtraHealth>() &&
                !extra->HasType<RE::ExtraTextDisplayData>() && !extra->HasType<RE::ExtraUniqueID>();
        }
        bool PlainWeapon(RE::ExtraDataList* extra) {
            if(!extra)return true;
            if(auto hotkey=extra->GetByType<RE::ExtraHotkey>();hotkey &&
                ImportantProvider() && static_cast<int>(hotkey->hotkey.underlying())==0xFA)return false;
            // Borrowed only during the game-thread inventory snapshot; no
            // mutating engine calls are made while walking these nodes.
            for(const auto& data:*extra)switch(data.GetType()) {
            case RE::ExtraDataType::kCount:case RE::ExtraDataType::kWorn:
            case RE::ExtraDataType::kWornLeft:case RE::ExtraDataType::kHotkey:break;
            default:return false; // Preserve ownership, charge, poison, names and unknown mod metadata.
            }
            return true;
        }
        std::vector<WeaponCopy> WeaponCopies(int count,RE::InventoryEntryData* entry) {
            InventoryBudget budget{count};std::vector<WeaponCopy> copies;
            if(entry && entry->extraLists)for(auto extra:*entry->extraLists)if(extra) {
                const int amount=budget.Take(extra->GetCount());if(amount<=0)continue;
                auto hotkey=extra->GetByType<RE::ExtraHotkey>();
                const int assigned=hotkey?static_cast<int>(hotkey->hotkey.underlying()):-1;
                copies.push_back({reinterpret_cast<std::uintptr_t>(extra),amount,PlainWeapon(extra),IsFavorite(hotkey),
                    extra->HasType<RE::ExtraWorn>(),extra->HasType<RE::ExtraWornLeft>(),assigned});
            }
            if(const int spare=budget.Spare();spare>0)copies.push_back({0,spare,true});
            return copies;
        }
        struct LiveEntry {RE::InventoryEntryData* entry=nullptr;int count=0;};
        LiveEntry FindInventory(RE::PlayerCharacter* player,RE::TESBoundObject* object) {
            RE::InventoryEntryData* entry=nullptr;std::int64_t count=0;
            if(auto changes=player->GetInventoryChanges();changes && changes->entryList)
                for(auto candidate:*changes->entryList)if(candidate && candidate->object==object){entry=candidate;count=candidate->countDelta;break;}
            if((!entry || !entry->IsLeveled()))if(auto container=player->GetContainer())
                container->ForEachContainerObject([&](RE::ContainerObject& base) {
                    if(base.obj==object)count+=base.count;
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            return {entry,static_cast<int>(std::clamp<std::int64_t>(count,0,INT_MAX))};
        }
        void Notify(const char* key) { RE::SendHUDMessage::ShowHUDMessage(Tr(Config(),key).c_str()); }
        RE::BGSEquipSlot* Hand(bool left) {
            const auto defaults = RE::BGSDefaultObjectManager::GetSingleton();
            return defaults ? defaults->GetObject<RE::BGSEquipSlot>(left ? RE::DEFAULT_OBJECT::kLeftHandEquip : RE::DEFAULT_OBJECT::kRightHandEquip) : nullptr;
        }
        struct SpellClearDone final:RE::BSScript::IStackCallbackFunctor {
            RE::FormID spell;bool left;
            SpellClearDone(RE::FormID form,bool hand):spell(form),left(hand){}
            void operator()(RE::BSScript::Variable)override {
                const auto form=spell;const auto hand=left;
                if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask([form,hand] {
                    auto player=RE::PlayerCharacter::GetSingleton();if(!player)return;
                    const auto& selected=player->GetActorRuntimeData().selectedSpells;
                    auto l=selected[RE::Actor::SlotTypes::kLeftHand],r=selected[RE::Actor::SlotTypes::kRightHand];
                    SKSE::log::info("Spell hand clear completed: form={:08X} cleared={} afterL={:08X} afterR={:08X}",
                        form,hand?"left":"right",l?l->GetFormID():0,r?r->GetFormID():0);
                });
            }
            void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&)override{}
        };
        bool ClearFavoriteSpell(RE::PlayerCharacter* player,RE::SpellItem* spell,bool left) {
            // Use the same registered native method as combat presets. Do not
            // edit selectedSpells directly or temporarily unpause the game.
            auto vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if(!vm || !vm->GetObjectHandlePolicy())return false;
            RE::BSScript::Variable actor;
            RE::BSScript::PackValue(&actor,static_cast<RE::Actor*>(player));
            if(!actor.IsObject())return false;
            auto object=actor.GetObject();if(!object)return false;
            RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new SpellClearDone(spell->GetFormID(),left));
            return vm->DispatchMethodCall(object,"UnequipSpell",
                RE::MakeFunctionArguments(static_cast<RE::SpellItem*>(spell),left?0:1),callback);
        }
        HandEquip InventoryHand(RE::TESBoundObject* object,bool wornRight,bool wornLeft) {
            if(auto weapon=object->As<RE::TESObjectWEAP>()) {
                const bool both=weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe() || weapon->IsBow() || weapon->IsCrossbow();
                return EquippedHand(wornRight,wornLeft,both);
            }
            if(auto scroll=object->As<RE::ScrollItem>())return EquippedHand(wornRight,wornLeft,scroll->IsTwoHanded());
            if(auto armor=object->As<RE::TESObjectARMO>();armor && armor->IsShield())return EquippedHand(wornRight,wornLeft,false,true);
            if(auto light=object->As<RE::TESObjectLIGH>();light && light->CanBeCarried())return EquippedHand(wornRight,wornLeft,false,true);
            return HandEquip::None; // Apparel/ammunition are equipped, but not in a hand.
        }
        void AppendInventory(std::vector<Item>& result,RE::TESBoundObject* object,int count,
            RE::InventoryEntryData* entry,bool allInventory,const Item* exact=nullptr) {
            if(!object || count<=0)return;
            const auto category=Classify(object);const auto icon=ClassifyIcon(object);const bool usable=Supported(object);
            const bool instrument=TalentInstrument(object);
            // A favorite marker may cover just one instrument. Keep ordinary copies for display grouping.
            const bool collectUnfavorited=allInventory || instrument;
            auto append=[&](RE::ExtraDataList* extra,int amount,const PlainWeaponStack* stack=nullptr) {
                if(amount<=0)return;
                const auto hotkey=extra?extra->GetByType<RE::ExtraHotkey>():nullptr;
                const bool favorite=IsFavorite(hotkey);
                if(!InventoryVisible(collectUnfavorited,favorite))return;
                if(exact) {
                    if(exact->weaponStack) {
                        if(!stack || stack->hotkey!=exact->weaponStackHotkey || (exact->favorited && !favorite))return;
                    }else if(reinterpret_cast<std::uintptr_t>(extra)!=exact->key.extra ||
                        Key(object,extra)!=exact->key || (exact->favorited && !favorite))return;
                }
                const char* name=extra?extra->GetDisplayName(object):object->GetName();
                if(!name || !*name)name=object->GetName();
                result.push_back({Key(object,extra),category,name && *name?name:"?",amount,
                    extra && (extra->HasType<RE::ExtraWorn>() || extra->HasType<RE::ExtraWornLeft>()),
                    false,usable,ActionKind::Favorite,0,icon});
                auto& item=result.back();item.iconForm=IconForm(object);item.scriptedInstrument=instrument;item.book=object->As<RE::TESObjectBOOK>()!=nullptr;SetSortClassification(item,object);item.favorited=favorite;item.inventoryWide=allInventory;
                item.handEquip=InventoryHand(object,extra && extra->HasType<RE::ExtraWorn>(),extra && extra->HasType<RE::ExtraWornLeft>());
                if(stack) {
                    item.weaponStack=true;item.weaponStackHotkey=stack->hotkey;item.equipped=stack->right || stack->left;
                    item.handEquip=InventoryHand(object,stack->right,stack->left);
                }
                if(hotkey){const int assigned=static_cast<int>(hotkey->hotkey.underlying());item.quickSlot=ValidQuickSlot(assigned)?assigned:-1;}
            };
            if(object->As<RE::TESObjectWEAP>()) {
                const auto copies=WeaponCopies(count,entry);
                const auto stack=BuildPlainWeaponStack(copies,exact && exact->weaponStack?std::optional<int>(exact->weaponStackHotkey):std::nullopt);
                if(stack.anchor>=0)append(reinterpret_cast<RE::ExtraDataList*>(copies[stack.anchor].extra),stack.count,&stack);
                for(const auto& copy:copies)if(!stack.Contains(copy))append(reinterpret_cast<RE::ExtraDataList*>(copy.extra),copy.count);
                return;
            }
            InventoryBudget budget{count};
            RE::ExtraDataList* anchor=nullptr;bool anchorFavorite=false;
            if(entry && entry->extraLists)for(auto extra:*entry->extraLists)if(extra) {
                const int amount=budget.Take(extra->GetCount());
                const bool favorite=IsFavorite(extra->GetByType<RE::ExtraHotkey>());
                if(amount>0 && (!anchor || (!anchorFavorite && favorite)) && InventoryVisible(collectUnfavorited,favorite) && (instrument?PlainInstrument(extra):Plain(extra))) {
                    // Preserve native favorites behavior; all-inventory merging
                    // additionally protects ownership/charge/poison/soul differences.
                    const bool compatible=(!allInventory && !instrument) || (!extra->HasType<RE::ExtraOwnership>() &&
                        !extra->HasType<RE::ExtraCharge>() && !extra->HasType<RE::ExtraPoison>() && !extra->HasType<RE::ExtraSoul>());
                    if(compatible){anchor=extra;anchorFavorite=favorite;}
                }
            }
            const int spare=budget.Spare();budget.remaining=count;
            if(entry && entry->extraLists)for(auto extra:*entry->extraLists)if(extra) {
                const int amount=budget.Take(extra->GetCount());
                append(extra,amount+(extra==anchor?spare:0));
            }
            if(collectUnfavorited && !anchor)append(nullptr,spare);
        }
        std::optional<Item> ResolveItem(const Item& requested) {
            if(!CurrentActionAllowed(requested))return {};
            auto player=RE::PlayerCharacter::GetSingleton();if(!player)return {};
            auto form=RE::TESForm::LookupByID(requested.key.form);if(!form)return {};
            if(requested.magic) {
                auto favorites=RE::MagicFavorites::GetSingleton();if(!favorites)return {};
                const auto spell=form->As<RE::SpellItem>();const auto shout=form->As<RE::TESShout>();
                if((!spell || !player->HasSpell(spell)) && (!shout || !player->HasShout(shout)))return {};
                if(!MagicCategory(shout!=nullptr,spell?spell->GetSpellType():RE::MagicSystem::SpellType::kVoicePower))return {};
                if(CurrentFormMode()!=FormMode::VampireLord &&
                    std::find(favorites->spells.begin(),favorites->spells.end(),form)==favorites->spells.end())return {};
                Item item=requested;item.quickSlot=-1;
                for(int i=0;i<nativeQuickSlots && i<static_cast<int>(favorites->hotkeys.size());++i)
                    if(favorites->hotkeys[i]==form){item.quickSlot=i;break;}
                return item;
            }
            const auto object=form->As<RE::TESBoundObject>();if(!object)return {};
            // Borrow only the matching form's entry, without allocating a whole
            // inventory map or cloning its instance-list nodes for each hover.
            const auto [entry,count]=FindInventory(player,object);
            if(count<=0)return {};
            std::vector<Item> candidates;
            AppendInventory(candidates,object,static_cast<int>(std::min<std::int64_t>(count,INT_MAX)),entry,requested.inventoryWide,&requested);
            for(auto& candidate:candidates)if((requested.weaponStack?candidate.weaponStack:candidate.key==requested.key) &&
                (!requested.favorited || candidate.favorited))return std::move(candidate);
            return {};
        }
    }

    static std::vector<Item> CollectDirectory(bool allInventory,bool includeInfo) {
        const auto started=std::chrono::steady_clock::now();
        std::vector<Item> result;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) return result;
        const auto formMode=CurrentFormMode();
        if(formMode==FormMode::UnsupportedBeast)return result;
        std::array<Ordering::Mode,categoryCount> sortModes{};
        if(formMode==FormMode::Human)for(int c=0;c<categoryCount;++c)
            sortModes[c]=Ordering::Current(allInventory,static_cast<Category>(c));
        // Borrow native entries only for this synchronous walk. Match CommonLib's
        // change/base-container count merge without cloning every extra-list node.
        struct Stock {std::int64_t count=0;RE::InventoryEntryData* entry=nullptr;};
        std::unordered_map<RE::TESBoundObject*,Stock> inventory;
        auto changes=formMode==FormMode::Human?player->GetInventoryChanges():nullptr;
        if(changes && changes->entryList)for(auto entry:*changes->entryList)
            if(entry && entry->object)inventory.emplace(entry->object,Stock{entry->countDelta,entry});
        if(auto container=formMode==FormMode::Human?player->GetContainer():nullptr)container->ForEachContainerObject([&](RE::ContainerObject& base) {
            if(base.obj) {
                auto& stock=inventory[base.obj];
                if(!stock.entry || !stock.entry->IsLeveled())stock.count+=base.count;
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
        result.reserve(inventory.size());
        for(const auto& [object,stock]:inventory) {
            if(stock.count<=0 || (!allInventory && (!stock.entry || !stock.entry->IsFavorited())))continue;
            const auto first=result.size();
            AppendInventory(result,object,static_cast<int>(std::min<std::int64_t>(stock.count,INT_MAX)),stock.entry,allInventory);
            const auto mode=sortModes[static_cast<int>(Classify(object))];
            if(includeInfo || Ordering::Numeric(mode))for(auto i=first;i<result.size();++i) {
                const auto stats=CaptureItemInfo(object,reinterpret_cast<RE::ExtraDataList*>(result[i].key.extra),player,includeInfo);
                if(Ordering::Numeric(mode)) {
                    result[i].sortNumber=mode==Ordering::Mode::WeightAscending?stats.weight:
                        mode==Ordering::Mode::ValueDescending?(stats.value?std::optional<float>{static_cast<float>(*stats.value)}:std::nullopt):
                        result[i].category==Category::Weapons?stats.damage:stats.armor;
                }
                if(includeInfo){result[i].info=stats;result[i].infoReady=true;}
            }
        }
        // Miscellaneous pickup instances often have separate UniqueIDs. Keep
        // those identities native, but combine equivalent read-only rows.
        {
            std::vector<Item> grouped;grouped.reserve(result.size());MiscDisplayStacks stacks;
            for(auto& item:result) {
                auto object=RE::TESForm::LookupByID(item.key.form);
                bool compatible=object && object->As<RE::TESObjectMISC>() && (!item.usable || item.scriptedInstrument);
                if(!compatible){grouped.push_back(std::move(item));continue;}
                auto extra=reinterpret_cast<RE::ExtraDataList*>(item.key.extra);
                int marker=-1;RE::FormID owner=0;
                if(extra)for(const auto& data:*extra)switch(data.GetType()) {
                case RE::ExtraDataType::kCount:case RE::ExtraDataType::kUniqueID:break;
                case RE::ExtraDataType::kHotkey:marker=static_cast<int>(extra->GetByType<RE::ExtraHotkey>()->hotkey.underlying());break;
                case RE::ExtraDataType::kOwnership: {
                    auto ownership=extra->GetByType<RE::ExtraOwnership>();
                    if(ownership->owner)owner=ownership->owner->GetFormID();
                    break;
                }
                default:compatible=false;break;
                }
                const bool instrument=item.scriptedInstrument;
                stacks.Append(grouped,std::move(item),compatible,marker,owner,instrument);
            }
            std::erase_if(grouped,[allInventory](const Item& item){return !InventoryVisible(allInventory,item.favorited);});
            result=std::move(grouped);
            for(const auto& item:result)if(item.scriptedInstrument)
                SKSE::log::info("SGT instrument display: form={:08X} count={} favorite={} grouped={} extra={:X}",
                    item.key.form,item.count,item.favorited,item.miscDisplayStack,item.key.extra);
        }
        if (auto favorites = RE::MagicFavorites::GetSingleton()) {
            std::vector<RE::TESForm*> magicForms;
            if(formMode==FormMode::VampireLord) {
                if(auto list=VampireSpellList())list->ForEachForm([&](RE::TESForm* form) {
                    if(AllowedVampireSpell(form) && std::find(magicForms.begin(),magicForms.end(),form)==magicForms.end())magicForms.push_back(form);
                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }else magicForms.assign(favorites->spells.begin(),favorites->spells.end());
            for (auto form : magicForms) {
                if (!form) continue;
                const auto spell = form->As<RE::SpellItem>();
                const auto shout = form->As<RE::TESShout>();
                if ((!spell || !player->HasSpell(spell)) && (!shout || !player->HasShout(shout))) continue;
                const auto category=MagicCategory(shout!=nullptr,spell?spell->GetSpellType():RE::MagicSystem::SpellType::kVoicePower);
                if(!category)continue;
                if(formMode==FormMode::Werewolf && *category!=Category::Powers && *category!=Category::Shouts)continue;
                const char* name = form->GetName();
                const bool wornRight=player->GetEquippedObject(false)==form,wornLeft=player->GetEquippedObject(true)==form;
                bool equipped = wornRight || wornLeft;
                if (auto defaults = RE::BGSDefaultObjectManager::GetSingleton()) {
                    if (auto slot = defaults->GetObject<RE::BGSEquipSlot>(RE::DEFAULT_OBJECT::kVoiceEquip))
                        equipped |= player->GetEquippedObjectInSlot(slot) == form;
                }
                result.push_back({{form->GetFormID()}, *category, name && *name ? name : "?", 1, equipped, true, true});
                result.back().inventoryWide=allInventory;result.back().iconForm=IconForm(form);
                if(spell) {
                    const auto school=spell->GetAssociatedSkill();
                    result.back().sortType=school==RE::ActorValue::kNone?999:static_cast<int>(school);
                    if(*category==Category::Spells)switch(school) {
                    case RE::ActorValue::kAlteration:result.back().icon=IconKind::Alteration;break;
                    case RE::ActorValue::kConjuration:result.back().icon=IconKind::Conjuration;break;
                    case RE::ActorValue::kDestruction:result.back().icon=IconKind::Destruction;break;
                    case RE::ActorValue::kIllusion:result.back().icon=IconKind::Illusion;break;
                    case RE::ActorValue::kRestoration:result.back().icon=IconKind::Restoration;break;
                    default:result.back().icon=IconKind::Magic;break;
                    }

                }
                if(spell && *category==Category::Spells && Ordering::Numeric(sortModes[static_cast<int>(*category)]))
                    result.back().sortNumber=ValidStat(spell->CalculateMagickaCost(player));
                if(spell && *category==Category::Spells)result.back().handEquip=EquippedHand(wornRight,wornLeft,spell->IsTwoHanded());
                for(int slot=0;slot<nativeQuickSlots && slot<static_cast<int>(favorites->hotkeys.size());++slot)
                    if(favorites->hotkeys[slot]==form){result.back().quickSlot=slot;break;}
                if(includeInfo){if(spell)result.back().info=CaptureSpellInfo(spell,player);result.back().infoReady=true;}
            }
        }
        std::sort(result.begin(), result.end(), [allInventory](const Item& a, const Item& b) {
            if (a.category != b.category) return a.category < b.category;
            if(allInventory) {
                if(a.equipped!=b.equipped)return a.equipped>b.equipped;
                if(a.favorited!=b.favorited)return a.favorited>b.favorited;
            }
            if (a.name != b.name) return a.name < b.name;
            if (a.key.form != b.key.form) return a.key.form < b.key.form;
            if (a.key.unique != b.key.unique) return a.key.unique < b.key.unique;
            return a.key.extra < b.key.extra;
        });
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
        SKSE::log::info("Inventory directory: scope={} items={} details={} total_ms={:.2f}",allInventory?"all":"favorites",result.size(),includeInfo,elapsed);
        return result;
    }

    std::vector<Item> CollectFavorites(bool includeInfo){return CollectDirectory(false,includeInfo);}
    std::vector<Item> CollectInventory(bool allInventory){return CollectDirectory(allInventory,false);}
    bool ReadItemInfo(const Item& requested,ItemInfo& info) {
        const auto found=ResolveItem(requested);if(!found)return false;
        auto player=RE::PlayerCharacter::GetSingleton();
        auto form=RE::TESForm::LookupByID(found->key.form);if(!player || !form)return false;
        if(found->magic) {if(auto spell=form->As<RE::SpellItem>())info=CaptureSpellInfo(spell,player);}
        else if(auto object=form->As<RE::TESBoundObject>())info=CaptureItemInfo(object,reinterpret_cast<RE::ExtraDataList*>(found->key.extra),player);
        return true;
    }

    bool BindFavoriteQuickSlot(const Item& requested,int slot,int* assignment) {
        if(CurrentFormMode()!=FormMode::Human)return false;
        if(requested.miscDisplayStack){Notify("unsupported");return false;}
        if(!ValidQuickSlot(slot) || requested.action!=ActionKind::Favorite)return false;
        auto player=RE::PlayerCharacter::GetSingleton();
        auto magic=RE::MagicFavorites::GetSingleton();
        if(!player || !magic || ActorRuntime::IsDead(player))return false;
        // Revalidate the exact inventory instance before using its borrowed extra list.
        const auto found=ResolveItem(requested);
        if(!found){Notify("changed");return false;}
        if(!found->favorited){Notify("quickSlotNeedsFavorite");return false;}
        RE::ExtraHotkey* target=nullptr;
        RE::TESForm* spell=nullptr;
        if(found->magic) {
            spell=RE::TESForm::LookupByID<RE::TESForm>(found->key.form);
            if(!spell || (!spell->As<RE::SpellItem>() && !spell->As<RE::TESShout>()))return false;
        } else {
            auto extra=reinterpret_cast<RE::ExtraDataList*>(found->key.extra);
            target=extra?extra->GetByType<RE::ExtraHotkey>():nullptr;
            if(!target)return false;
        }
        const int next=AssignedQuickSlot(found->quickSlot,slot);
        // Allocate the magic-slot array before modifying anything else.
        // Do not truncate slots owned by extensions beyond the native eight.
        if(spell && next>=0 && magic->hotkeys.size()<nativeQuickSlots)magic->hotkeys.resize(nativeQuickSlots);
        bool inventoryChanged=false;
        auto changes=player->GetInventoryChanges();
        if(changes && changes->entryList)for(auto entry:*changes->entryList) {
            if(!entry || !entry->object || !entry->extraLists)continue;
            for(auto extra:*entry->extraLists)if(extra)if(auto hotkey=extra->GetByType<RE::ExtraHotkey>()) {
                const int old=static_cast<int>(hotkey->hotkey.underlying());
                const int replacement=ReassignedQuickSlot(old,hotkey==target,slot,next);
                if(replacement!=old) {
                    // kUnbound keeps the ExtraHotkey: removing it would also unfavorite the item.
                    hotkey->hotkey=static_cast<RE::ExtraHotkey::Hotkey>(replacement);
                    inventoryChanged=true;
                }
            }
        }
        if(inventoryChanged)player->AddChange(static_cast<std::uint32_t>(RE::TESObjectREFR::ChangeFlags::kInventory));
        for(std::size_t i=0;i<std::min<std::size_t>(magic->hotkeys.size(),nativeQuickSlots);++i)
            if(i==static_cast<std::size_t>(slot) || (spell && magic->hotkeys[i]==spell))magic->hotkeys[i]=nullptr;
        if(spell && next>=0)magic->hotkeys[next]=spell;
        if(assignment)*assignment=next;
        SKSE::log::info("Favorite quick slot: form={:08X} extra={:X} key={} assignment={}",
            found->key.form,found->key.extra,slot+1,next<0?"unbound":"bound");
        return true;
    }

    bool FavoriteQuickSlotOccupied(int slot) {
        if(!ValidQuickSlot(slot))return false;
        const auto magic=RE::MagicFavorites::GetSingleton();
        if(magic && slot<static_cast<int>(magic->hotkeys.size()) && magic->hotkeys[slot])return true;
        const auto player=RE::PlayerCharacter::GetSingleton();
        auto changes=player?player->GetInventoryChanges():nullptr;
        if(changes && changes->entryList)for(auto entry:*changes->entryList) {
            if(!entry || !entry->extraLists)continue;
            for(auto extra:*entry->extraLists)if(extra)if(auto hotkey=extra->GetByType<RE::ExtraHotkey>())
                if(static_cast<int>(hotkey->hotkey.underlying())==slot)return true;
        }
        return false;
    }
    bool ClearFavoriteQuickSlot(int slot) {
        if(CurrentFormMode()!=FormMode::Human)return false;
        if(!ValidQuickSlot(slot))return false;
        auto player=RE::PlayerCharacter::GetSingleton();auto magic=RE::MagicFavorites::GetSingleton();
        if(!player || !magic || ActorRuntime::IsDead(player))return false;
        bool changed=false;
        auto changes=player->GetInventoryChanges();
        if(changes && changes->entryList)for(auto entry:*changes->entryList) {
            if(!entry || !entry->extraLists)continue;
            for(auto extra:*entry->extraLists)if(extra)if(auto hotkey=extra->GetByType<RE::ExtraHotkey>())
                if(static_cast<int>(hotkey->hotkey.underlying())==slot) {
                    hotkey->hotkey=RE::ExtraHotkey::Hotkey::kUnbound;changed=true;
                }
        }
        if(changed)player->AddChange(static_cast<std::uint32_t>(RE::TESObjectREFR::ChangeFlags::kInventory));
        if(slot<static_cast<int>(magic->hotkeys.size()))magic->hotkeys[slot]=nullptr;
        SKSE::log::info("Favorite quick slot cleared for combat preset: slot={}",slot+1);
        return true;
    }
    void UseFavorite(const Item& requested, bool leftHand) {
        if(!CurrentActionAllowed(requested))return;
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto manager = RE::ActorEquipManager::GetSingleton();
        if (!player || !manager || ActorRuntime::IsDead(player)) return;
        // Resolve again at execution time. A queued action must never use an expired extra-list pointer.
        const auto found = ResolveItem(requested);
        if (!found) {
            Notify("changed");
            SKSE::log::warn("Favorite no longer matches: {:08X} extra={:X} stack={} marker={}",
                requested.key.form,requested.key.extra,requested.weaponStack,requested.weaponStackHotkey);
            return;
        }
        if (!found->usable) { Notify("unsupported"); return; }
        const auto form = RE::TESForm::LookupByID(requested.key.form);
        if (!form) return;
        if (auto shout = form->As<RE::TESShout>()) { manager->EquipShout(player, shout); return; }
        if (auto spell = form->As<RE::SpellItem>()) {
            auto slot = spell->GetEquipSlot();
            if(CurrentFormMode()==FormMode::VampireLord) {
                // Respect the beast spell's authored slot; never clear/transfer
                // human dual-wield selections or force a requested mouse hand.
                if(slot)manager->EquipSpell(player,spell,slot);
                return;
            }
            const auto type = spell->GetSpellType();
            const bool twoHands=spell->IsTwoHanded();
            if(type==RE::MagicSystem::SpellType::kSpell && !twoHands)slot=Hand(leftHand);
            const auto selected=[&](bool left) {
                auto selected=player->GetActorRuntimeData().selectedSpells[left?RE::Actor::SlotTypes::kLeftHand:RE::Actor::SlotTypes::kRightHand];
                return selected?selected->GetFormID():0u;
            };
            const auto beforeLeft=selected(true),beforeRight=selected(false);
            const auto action=SpellHandPlan(type==RE::MagicSystem::SpellType::kSpell,twoHands,leftHand,
                beforeLeft==spell->GetFormID(),beforeRight==spell->GetFormID());
            struct SpellOperations {
                RE::PlayerCharacter* player;RE::ActorEquipManager* manager;RE::SpellItem* spell;RE::BGSEquipSlot* slot;
                bool Clear(bool left){return ClearFavoriteSpell(player,spell,left);}
                bool Equip(bool) {
                    if(!slot)return false;
                    manager->EquipSpell(player,spell,slot);return true;
                }
            } operations{player,manager,spell,slot};
            const bool submitted=ExecuteSpellHand(action,leftHand,operations);
            if(!submitted)Notify("changed");
            SKSE::log::info("Spell hand request: form={:08X} name='{}' target={} action={} submitted={} type={} twoHanded={} slot={:08X} beforeL={:08X} beforeR={:08X} afterL={:08X} afterR={:08X}",
                spell->GetFormID(),found->name,leftHand?"left":"right",action.Unequip()?"unequip":"equip",submitted,static_cast<int>(type),twoHands,slot?slot->GetFormID():0,
                beforeLeft,beforeRight,selected(true),selected(false));
            return;
        }
        auto object = form->As<RE::TESBoundObject>();
        if (!object) return;
        // This address was matched against a fresh inventory walk above, on this same game-thread task.
        auto extra = reinterpret_cast<RE::ExtraDataList*>(found->key.extra);
        if(TalentInstrument(object)) {
            // Match inventory ToggleEquipItem: the queued Actor path drops MISC
            // before its native TESEquipEvent can be sent.
            manager->EquipObject(player,object,extra,1,nullptr,false,false,true,false);
            SKSE::log::info("SGT instrument native use requested: form={:08X} extra={:X} queueEquip=false count=1",
                object->GetFormID(),found->key.extra);
            return;
        }
        if(auto book=object->As<RE::TESObjectBOOK>()) {
            if(book->TeachesSpell()) {
                const bool learned=book->Read(player);
                if(learned)player->RemoveItem(book,1,RE::ITEM_REMOVE_REASON::kRemove,extra,nullptr);
                SKSE::log::info("Book spell learning: form={:08X} learned={}",book->GetFormID(),learned);
            }else {
                RE::NiMatrix3 rotation{};rotation.SetEulerAnglesXYZ(-0.05f,-0.05f,1.50f);
                RE::BookMenu::OpenMenuFromBaseForm(book,extra,RE::NiPoint3(),rotation,1.0f,true);
                SKSE::log::info("Book menu requested: form={:08X} extra={:X}",book->GetFormID(),found->key.extra);
            }
            return;
        }
        if (auto potion = object->As<RE::AlchemyItem>()) {
            if (potion->IsPoison()) {
                // Vanilla handles the weapon checks, confirmation and consumption.
                // The wheel has closed before entering this consumable path.
                player->UsePoisonFromInventory(potion);
                SKSE::log::info("Submitted poison {:08X} through UsePoisonFromInventory", found->key.form);
                return;
            }
            // Use the engine equip entry point: UAPNG hooks this path, and EAS
            // listens for OnObjectEquipped. Never also call DrinkPotion or send
            // a synthetic equip event: animation mods may defer consumption.
            manager->EquipObject(player, object, extra, 1);
            SKSE::log::info("Submitted consumable {:08X} through EquipObject (food={}, count=1)", found->key.form, potion->IsFood());
            return;
        }
        RE::BGSEquipSlot* slot = nullptr;
        bool unequipHand=false;
        if (auto weapon = object->As<RE::TESObjectWEAP>()) {
            const bool twoHands = weapon->IsTwoHandedSword() || weapon->IsTwoHandedAxe() || weapon->IsBow() || weapon->IsCrossbow();
            slot = twoHands ? weapon->GetEquipSlot() : Hand(leftHand);
            if(!slot)return;
            auto action=WeaponHandPlan(twoHands,leftHand,extra && extra->HasType<RE::ExtraWorn>(),
                extra && extra->HasType<RE::ExtraWornLeft>(),found->count);
            Item nativeItem=*found;
            if(found->weaponStack) {
                const auto live=FindInventory(player,object);
                const auto copies=WeaponCopies(live.count,live.entry);
                const auto stack=BuildPlainWeaponStack(copies,found->weaponStackHotkey);
                const auto plan=PlanStackWeapon(copies,stack,leftHand,twoHands);
                if(plan.index<0 || (found->favorited && !stack.favorite)){Notify("changed");return;}
                const auto& copy=copies[plan.index];action=plan.action;
                auto selected=reinterpret_cast<RE::ExtraDataList*>(plan.nativeSpare?0:copy.extra);
                nativeItem.key=Key(object,selected);nativeItem.count=copy.count;
                SKSE::log::info("Weapon stack selection: form={:08X} target={} poolCount={} copies={} anchor={:X} selected={:X} physicalCount={} nativeSpare={}",
                    found->key.form,leftHand?"left":"right",stack.count,copies.size(),found->key.extra,nativeItem.key.extra,copy.count,plan.nativeSpare);
            }else {
                std::string types;
                if(extra)for(const auto& data:*extra) {
                    if(!types.empty())types+=',';
                    types+=std::to_string(static_cast<unsigned>(data.GetType()));
                }
                SKSE::log::info("Weapon exact selection: form={:08X} extra={:X} count={} types='{}'",
                    found->key.form,found->key.extra,found->count,types);
            }
            struct Operations {
                RE::PlayerCharacter* player;RE::ActorEquipManager* manager;RE::TESBoundObject* object;
                Item item;RE::BGSEquipSlot* nativeSlot;bool twoHanded,plain,targetLeft;
                RE::BGSEquipSlot* Slot(bool left)const{return twoHanded?nativeSlot:Hand(left);}
                void Unequip(bool left) {
                    auto slot=Slot(left);if(!slot)return;
                    // false normally means the extra list survived successful
                    // unequip. It must not abort the following transfer.
                    manager->UnequipObject(player,object,reinterpret_cast<RE::ExtraDataList*>(item.key.extra),1,slot,false,false,true,true);
                }
                bool Refresh() {
                    // Unequipping may merge/delete an empty extra list. Never
                    // reuse its address as a live pointer for the equip call.
                    auto fresh=ResolveItem(item);
                    if(!fresh && plain) {
                        auto stack=item;stack.key.extra=0;
                        fresh=ResolveItem(stack);
                    }
                    if(!fresh)return false;
                    item=std::move(*fresh);
                    auto current=reinterpret_cast<RE::ExtraDataList*>(item.key.extra);
                    return !current || !(targetLeft?current->HasType<RE::ExtraWorn>():current->HasType<RE::ExtraWornLeft>());
                }
                bool Equip(bool left) {
                    auto slot=Slot(left);if(!slot)return false;
                    manager->EquipObject(player,object,reinterpret_cast<RE::ExtraDataList*>(item.key.extra),1,slot,false,false,true,true);
                    return true; // Submitted; native events update the directory.
                }
            } operations{player,manager,object,nativeItem,slot,twoHands,
                extra && Plain(extra) && !extra->HasType<RE::ExtraOwnership>() && !extra->HasType<RE::ExtraCharge>() &&
                    !extra->HasType<RE::ExtraPoison>() && !extra->HasType<RE::ExtraSoul>(),leftHand};
            const bool submitted=ExecuteWeaponHand(action,leftHand,operations);
            if(!submitted)Notify("changed");
            const auto actualLeft=player->GetEquippedObject(true),actualRight=player->GetEquippedObject(false);
            SKSE::log::info("Weapon hand request: form={:08X} target={} action={} count={} submitted={} beforeExtra={:X} afterExtra={:X} afterL={:08X} afterR={:08X}",
                found->key.form,leftHand?"left":"right",action==WeaponHandAction::Move?"move":action==WeaponHandAction::Unequip?"unequip":"equip",
                found->count,submitted,found->key.extra,operations.item.key.extra,
                actualLeft?actualLeft->GetFormID():0,actualRight?actualRight->GetFormID():0);
            return;
        } else if (auto scroll = object->As<RE::ScrollItem>()) {
            slot = scroll->IsTwoHanded() ? scroll->GetEquipSlot() : Hand(leftHand);
        } else if (auto light = object->As<RE::TESObjectLIGH>()) {
            if(!light->CanBeCarried())return;
            // Carryable lights always occupy the left hand, for either use button.
            slot = Hand(true);
            if(!slot)return;
            unequipHand=found->equipped;
        }
        // Match the freshly resolved instance and requested hand, not just the base FormID.
        if (unequipHand || (object->As<RE::TESObjectARMO>() && found->equipped))
            manager->UnequipObject(player, object, extra, 1, unequipHand?slot:nullptr, false, false, true, true);
        else
            manager->EquipObject(player, object, extra, object->As<RE::TESAmmo>() ? found->count : 1, slot, false, false, true, true);
        SKSE::log::info("Used favorite {:08X}, left={}, extra={:X}", found->key.form, leftHand, found->key.extra);
    }
}

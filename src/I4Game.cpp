#include "I4Game.h"
#include "I4Diagnostics.h"
#include "I4LoadLog.h"
#include "I4Textures.h"
#include <filesystem>
#include <chrono>
#include <set>

namespace Wheel::I4 {
namespace {
    std::filesystem::path CacheDirectory() {
        std::wstring executable(32768,L'\0');
        const auto size=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
        if(!size || size>=executable.size())return {};
        executable.resize(size);
        return std::filesystem::path(executable).parent_path()/L"Data/SKSE/Plugins/FavoriteWheel/IconCache";
    }
    bool SafeSource(std::string_view name) {
        const auto key=SourceKey(std::string(name));
        if(key.empty() || key.front()=='/' || key.find(':')!=key.npos || key.find('\0')!=key.npos)return false;
        const std::filesystem::path path(key);
        for(const auto& part:path)if(part=="..")return false;
        return true;
    }
    std::optional<std::vector<std::uint8_t>> ReadSource(const std::string& source) {
        if(!SafeSource(source))return {};
        RE::BSResourceNiBinaryStream stream("Interface/"+source);
        if(!stream.good() || !stream.stream)return {};
        const auto size=stream.stream->totalSize;
        if(size<8 || size>64*1024*1024)return {};
        std::vector<std::uint8_t> bytes(size);
        if(!stream.read(bytes.data(),size))return {};
        return bytes;
    }
    std::optional<Value::Keywords> Keywords(const RE::TESForm* form) {
        if(!form)return {};
        const auto keywords=form->As<RE::BGSKeywordForm>();if(!keywords)return {};
        Value::Keywords result;
        // Runtime keyword arrays include KID distributions; never consult base records on disk.
        for(const auto keyword:keywords->GetKeywords())if(keyword) {
            const auto id=keyword->GetFormEditorID();if(id && *id)result.insert(id);
        }
        return result;
    }
}
LoadReport LoadFromGame() {
    const auto started=std::chrono::steady_clock::now();
    LoadReport report;
    try {
        I4Textures::Configure(ReadSource,[](std::function<void()> task) {
            if(auto tasks=SKSE::GetTaskInterface())tasks->AddTask(std::move(task));
            else throw std::runtime_error("SKSE task interface unavailable");
        },[](const std::string& message){SKSE::log::warn("{}",message);},CacheDirectory());
        auto handler=RE::TESDataHandler::GetSingleton();
        if(!handler)throw std::runtime_error("TESDataHandler unavailable");
        std::map<std::string,std::uint32_t> prefixes;
        std::vector<std::pair<std::string,std::string>> files;
        std::vector<std::string> diagnostics;
        const auto directory=std::filesystem::path("SKSE/Plugins/InventoryInjector");
        // Match InventoryInjector config discovery: checked flag, not assigned index.
        for(const auto file:handler->files)if(file && file->recordFlags.all(RE::TESFile::RecordFlag::kChecked)) {
            const std::string name(file->GetFilename());
            if(file->compileIndex!=0xFF)prefixes[SourceKey(name)]=file->IsLight()?0xFE000000u|(std::uint32_t(file->smallFileCompileIndex)<<12):std::uint32_t(file->compileIndex)<<24;
            // I4 uses plugin basename + .json, e.g. Skyrim.esm -> Skyrim.json.
            auto json=std::filesystem::path(name);json.replace_extension(".json");
            const auto path=directory/json;
            RE::BSResourceNiBinaryStream stream(path.generic_string());
            if(!stream.good() || !stream.stream)continue;
            const auto size=stream.stream->totalSize;
            if(size>16*1024*1024) {diagnostics.push_back("I4 oversized JSON: "+path.generic_string());continue;}
            std::string text(size,'\0');
            if(size && !stream.read(text.data(),size)) {diagnostics.push_back("I4 cannot read JSON: "+path.generic_string());continue;}
            files.emplace_back(name,std::move(text));
        }
        if(GetModuleHandleW(L"MergeMapper.dll"))diagnostics.push_back("I4 MergeMapper detected: merged-plugin remapping is not implemented in T-4");
        std::set<std::string> missing;
        std::map<std::string,bool> existence;
        LoadOptions options;
        options.pluginPrefix=[&](std::string_view plugin)->std::optional<std::uint32_t> {
            const auto key=SourceKey(std::string(plugin));
            if(const auto found=prefixes.find(key);found!=prefixes.end())return found->second;
            if(missing.insert(key).second)diagnostics.push_back("I4 unresolved/unloaded plugin (possibly merged; no remapping): "+std::string(plugin));
            return {};
        };
        options.sourceExists=[&](std::string_view source) {
            const auto key=SourceKey(std::string(source));
            if(auto found=existence.find(key);found!=existence.end())return found->second;
            bool exists=false;
            if(SafeSource(key)) {RE::BSResourceNiBinaryStream stream("Interface/"+key);exists=stream.good();}
            existence[key]=exists;return exists;
        };
        report=Load(files,options);
        report.warnings.insert(report.warnings.end(),diagnostics.begin(),diagnostics.end());
        // AC-8: no rule files means no SkyUI default-icon substitution either.
        dataAvailable.store(report.files!=0,std::memory_order_release);
    } catch(const std::exception& error) {dataAvailable.store(false);report.warnings.push_back(error.what());}
    const auto log=SummarizeLoadWarnings(report.warnings);
    for(const auto& warning:log.warnings)SKSE::log::warn("I4 load: {}",warning);
    if(log.missingPlugins)SKSE::log::info("I4 load: {} references to {} missing plugins (rules kept; these IDs never match)",
        log.missingReferences,log.missingPlugins);
    SKSE::log::info("I4 load: files={} rules={} rejected={} elapsed_ms={:.3f}",report.files,report.rules,report.rejected,
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count());
    return report;
}
Icon ResolveItem(Entry& entry,std::uint32_t formId,std::string_view name) {
    if(!Runtime().debug)return Resolve(entry);
    std::vector<RuleMatch> matches;
    auto icon=Resolve(entry,&matches);
    SKSE::log::info("{}",FormatResolveDebug(formId,name,icon,matches));
    return icon;
}
RawFields Collect(const RE::TESForm* form,const RE::InventoryEntryData* entry) {
    RawFields raw;if(!form)return raw;
    raw.formType=static_cast<std::uint32_t>(form->GetFormType());raw.formId=form->GetFormID();raw.keywords=Keywords(form);
    if(const auto equip=form->As<RE::BGSEquipType>();equip && equip->GetEquipSlot())raw.equipSlot=equip->GetEquipSlot()->GetFormID();
    if(const auto armor=form->As<RE::TESObjectARMO>()) {
        raw.partMask=armor->GetSlotMask().underlying();raw.armorType=static_cast<double>(armor->GetArmorType());
    }
    if(const auto weapon=form->As<RE::TESObjectWEAP>())raw.weaponType=static_cast<double>(weapon->GetWeaponType());
    if(const auto book=form->As<RE::TESObjectBOOK>()) {
        raw.flags=book->data.flags.underlying();raw.bookType=book->data.type.underlying();
        // InventoryInjector@52cc4cb IconSetter::FixNote (no file access).
        const auto model=SourceKey(std::filesystem::path(book->model.c_str()).stem().string());
        if(model.find("note")!=model.npos)raw.bookType=255;
        if(book->data.flags.all(RE::OBJ_BOOK::Flag::kTeachesSpell)) {
            if(book->data.teaches.spell)raw.teachesSpell=book->data.teaches.spell->GetFormID();
        } else if(book->data.flags.all(RE::OBJ_BOOK::Flag::kAdvancesActorValue))raw.teachesSkill=static_cast<double>(book->data.teaches.actorValueToAdvance);
    }
    if(const auto ammo=form->As<RE::TESAmmo>())raw.flags=ammo->GetRuntimeData().data.flags.underlying();
    if(const auto gem=form->As<RE::TESSoulGem>()) {
        raw.gemSize=static_cast<double>(gem->GetMaximumCapacity());
        raw.soulSize=static_cast<double>(entry?entry->GetSoulLevel():gem->GetContainedSoul());
    }
    if(const auto potion=form->As<RE::AlchemyItem>()) {
        raw.flags=potion->data.flags.underlying();
        if(potion->data.consumptionSound)raw.useSoundFormId=potion->data.consumptionSound->GetFormID();
    }
    if(const auto magic=form->As<RE::MagicItem>()) {
        raw.spellType=static_cast<double>(magic->GetSpellType());raw.castType=static_cast<double>(magic->GetCastingType());
        raw.deliveryType=static_cast<double>(magic->GetDelivery());
        // Scalar effect fields use the costliest effect. InventoryInjector@52cc4cb
        // IconSetter::ExtendMagicItemData unions keywords from ALL effects.
        raw.effectKeywords=Value::Keywords{};
        for(const auto effect:magic->effects)if(effect && effect->baseEffect)
            if(const auto keys=Keywords(effect->baseEffect))raw.effectKeywords->insert(keys->begin(),keys->end());
        if(const auto effect=magic->GetCostliestEffectItem();effect && effect->baseEffect) {
            const auto base=effect->baseEffect;
            raw.school=static_cast<double>(base->data.associatedSkill);raw.resistance=static_cast<double>(base->data.resistVariable);
            raw.actorValue=static_cast<double>(base->data.primaryAV);raw.archetype=static_cast<double>(base->data.archetype);
            raw.effectFlags=base->data.flags.underlying();raw.skillLevel=base->data.minimumSkill;
        }
    }
    if(const auto spell=form->As<RE::SpellItem>()) {
        raw.magicMenu=true;raw.type=spell->GetSpellType()==RE::MagicSystem::SpellType::kSpell?7:13;
    }
    if(form->As<RE::TESShout>()) {raw.magicMenu=true;raw.type=10;}
    return raw;
}
}

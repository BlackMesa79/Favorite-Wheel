#pragma once
#include "FormMode.h"
#include "Favorites.h"
#include "MagicCategories.h"
#include <RE/Skyrim.h>
namespace Wheel {
    inline FormMode CurrentFormMode() {
        auto player=RE::PlayerCharacter::GetSingleton();
        auto menus=RE::MenuControls::GetSingleton();
        auto defaults=RE::BGSDefaultObjectManager::GetSingleton();
        auto race=defaults?defaults->GetObject<RE::TESRace>(RE::DEFAULT_OBJECT::kVampireRace):nullptr;
        return ClassifyFormMode(menus && menus->InBeastForm(),player && race && player->GetRace()==race);
    }
    inline RE::BGSListForm* VampireSpellList() {
        auto defaults=RE::BGSDefaultObjectManager::GetSingleton();
        return defaults?defaults->GetObject<RE::BGSListForm>(RE::DEFAULT_OBJECT::kVampireSpells):nullptr;
    }
    inline bool AllowedVampireSpell(RE::TESForm* form) {
        auto player=RE::PlayerCharacter::GetSingleton();
        auto spell=form?form->As<RE::SpellItem>():nullptr;
        auto list=VampireSpellList();
        return player && spell && list && list->HasForm(spell) && player->HasSpell(spell) &&
            MagicCategory(false,spell->GetSpellType()).has_value();
    }
    inline bool CurrentActionAllowed(const Item& item) {
        const auto mode=CurrentFormMode();
        return FormActionAllowed(mode,item.action==ActionKind::Favorite,
            mode==FormMode::VampireLord && item.magic && AllowedVampireSpell(RE::TESForm::LookupByID(item.key.form)));
    }
}

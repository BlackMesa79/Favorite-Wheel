#pragma once
#include "I4Rules.h"

namespace Wheel::I4 {
    struct RawFields {
        std::uint32_t formType = 0, formId = 0;
        // Missing is distinct from numeric zero. No engine pointers or CommonLib headers.
        std::optional<Value::Keywords> keywords, effectKeywords;
        std::optional<std::uint32_t> partMask, flags, effectFlags, equipSlot, teachesSpell;
        std::optional<double> armorType, weaponType, bookType, teachesSkill;
        std::optional<double> gemSize, soulSize, status, school, resistance, actorValue;
        std::optional<double> archetype, deliveryType, castType, spellType, skillLevel;
        std::optional<std::uint32_t> useSoundFormId;
        // SkyUI a_itemInfo.type, not spellType. MagicDataSetter is selected by the menu.
        std::optional<double> type;
        bool magicMenu = false;
    };
    Entry Derive(const RawFields& raw);
    // Collect and LoadFromGame belong to the game adapter in phase 3.
}

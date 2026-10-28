#pragma once
namespace Wheel {
    enum class FormMode { Human, VampireLord, UnsupportedBeast };
    constexpr FormMode ClassifyFormMode(bool beast, bool vampireRace) {
        return vampireRace ? FormMode::VampireLord : beast ? FormMode::UnsupportedBeast : FormMode::Human;
    }
    constexpr bool FormActionAllowed(FormMode mode,bool favorite,bool allowedSpell) {
        return mode==FormMode::Human || (mode==FormMode::VampireLord && favorite && allowedSpell);
    }
}

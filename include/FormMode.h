#pragma once
namespace Wheel {
    enum class FormMode { Human, VampireLord, Werewolf, UnsupportedBeast };
    constexpr FormMode ClassifyFormMode(bool beast, bool vampireRace, bool werewolfRace=false) {
        return vampireRace ? FormMode::VampireLord : werewolfRace ? FormMode::Werewolf : beast ? FormMode::UnsupportedBeast : FormMode::Human;
    }
    constexpr bool FormActionAllowed(FormMode mode,bool favorite,bool allowedSpell,bool voiceItem=false) {
        return mode==FormMode::Human || (mode==FormMode::VampireLord && favorite && allowedSpell) ||
            (mode==FormMode::Werewolf && favorite && allowedSpell && voiceItem);
    }
}

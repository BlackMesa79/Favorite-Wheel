#pragma once
#include <cstdint>
namespace Wheel {
    enum class ActionDecision { Wait, Submit, Discard };
    enum class ActionExecutor { TaskQueue, PlayerUpdate };
    constexpr bool KeepWheelAfterUse(bool enabled,bool consumable,bool retainConsumable=false,bool opensMenu=false) {return !opensMenu && (consumable?retainConsumable:enabled);}
    constexpr bool KeepPausedForUse(bool retained,bool favorite,bool consumable,int timeMode) {
        return retained && favorite && !consumable && timeMode==0;
    }
    constexpr bool OnlyWheelPause(bool guardOpen,bool guardPauses,std::uint32_t pauseCount) {
        return guardOpen && guardPauses && pauseCount==1;
    }
    constexpr ActionExecutor ExecutorFor(bool faceLightCommand) {
        return faceLightCommand ? ActionExecutor::PlayerUpdate : ActionExecutor::TaskQueue;
    }
    // A queue may only consume its own work, including cancellations/timeouts.
    // SKSE tasks are not necessarily on the thread recognized by Face Lighting.
    inline ActionDecision DecideActionOn(ActionExecutor caller, ActionExecutor owner,
        std::uint64_t expected, std::uint64_t current, bool valid, bool wheelClosing,
        bool wheelOpen, bool paused, bool expired) {
        if (caller != owner) return ActionDecision::Wait;
        if (expected != current || !valid || expired) return ActionDecision::Discard;
        if (wheelClosing || wheelOpen || paused) return ActionDecision::Wait;
        return ActionDecision::Submit;
    }
    // Immediate favorite equipment may pass our own single pause. Other
    // executors and foreign pauses still wait for the normal unpaused path.
    inline ActionDecision DecideRetainedActionOn(ActionExecutor caller,ActionExecutor owner,
        std::uint64_t expected,std::uint64_t current,bool valid,bool closing,bool menuOpen,
        bool paused,bool expired,bool retained,bool sameSession,bool viewOpen,
        bool keepPaused=false,bool onlyWheelPause=false) {
        if(caller!=owner)return ActionDecision::Wait;
        if(retained && viewOpen && !sameSession)return ActionDecision::Discard;
        const bool ownPauseAllowed=keepPaused && onlyWheelPause && retained && sameSession && viewOpen &&
            owner==ActionExecutor::TaskQueue;
        return DecideActionOn(caller,owner,expected,current,valid,closing,
            menuOpen && !(retained && sameSession && viewOpen),paused && !ownPauseAllowed,expired);
    }
    inline bool RetainedActionSettled(bool pending,bool outfitBusy,double elapsed,std::uint64_t updates) {
        // Give native equip queues a subsequent player update before pausing
        // again; wall time alone is insufficient when another menu freezes us.
        return !pending && !outfitBusy && elapsed>=.15 && updates>=2;
    }
    inline ActionDecision DecideAction(std::uint64_t expected, std::uint64_t current,
        bool valid, bool wheelClosing, bool wheelOpen, bool paused, bool expired) {
        return DecideActionOn(ActionExecutor::TaskQueue, ActionExecutor::TaskQueue,
            expected, current, valid, wheelClosing, wheelOpen, paused, expired);
    }
}

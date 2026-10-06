#pragma once
#include <cstdint>
namespace Wheel {
    enum class ActionDecision { Wait, Submit, Discard };
    enum class ActionExecutor { TaskQueue, PlayerUpdate };
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
    inline ActionDecision DecideAction(std::uint64_t expected, std::uint64_t current,
        bool valid, bool wheelClosing, bool wheelOpen, bool paused, bool expired) {
        return DecideActionOn(ActionExecutor::TaskQueue, ActionExecutor::TaskQueue,
            expected, current, valid, wheelClosing, wheelOpen, paused, expired);
    }
}

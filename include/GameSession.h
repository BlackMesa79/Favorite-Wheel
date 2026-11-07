#pragma once
#include <atomic>
#include <cstdint>

namespace Wheel {
struct StartupWorld {
    bool dataLoaded=false,uiAvailable=false,startupMenuOpen=true;
    bool playerPresent=false,player3D=false,cellAttached=false,playerLoading=true;
    bool Ready() const {
        return dataLoaded && uiAvailable && !startupMenuOpen && playerPresent &&
            player3D && cellAttached && !playerLoading;
    }
};
// Only startup without an explicit SKSE session notification may infer a world.
// PreLoadGame or a failed PostLoadGame must not reactivate a stale player.
class GameSession {
    enum class State { AwaitingWorld,Inactive,Active };
    std::atomic<State> state{State::AwaitingWorld};
public:
    bool Active() const {return state.load()==State::Active;}
    bool AwaitingWorld() const {return state.load()==State::AwaitingWorld;}
    void Notify(bool active) {state.store(active?State::Active:State::Inactive);}
    bool CanBootstrap(const StartupWorld& world,std::uint64_t expected,std::uint64_t current) const {
        return AwaitingWorld() && expected==current && world.Ready();
    }
};
}

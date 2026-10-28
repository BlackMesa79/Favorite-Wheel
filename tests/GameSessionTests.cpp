#include "GameSession.h"
#include <cstdlib>
#include <iostream>

void Check(bool result,const char* message) {
    if(!result){std::cerr<<message<<'\n';std::exit(1);}
}
int main() {
    using namespace Wheel;
    GameSession session;
    const StartupWorld ready{true,true,false,true,true,true,false};
    Check(!session.Active() && session.AwaitingWorld(),"Startup remains inactive until an actual session exists");
    Check(!session.CanBootstrap(StartupWorld{},0,0),"An uninitialized title screen cannot activate gameplay");
    auto blocked=[&](auto edit,const char* message) {
        auto candidate=ready;edit(candidate);Check(!session.CanBootstrap(candidate,1,1),message);
    };
    blocked([](auto& w){w.dataLoaded=false;},"No early activation before DataLoaded");
    blocked([](auto& w){w.uiAvailable=false;},"No early activation without UI state");
    blocked([](auto& w){w.startupMenuOpen=true;},"Main/loading/console/character-creation menus delay activation even with a player object");
    blocked([](auto& w){w.playerPresent=false;},"Missing player cannot activate");
    blocked([](auto& w){w.player3D=false;},"A player singleton alone is insufficient");
    blocked([](auto& w){w.cellAttached=false;},"Wait for an attached world cell");
    blocked([](auto& w){w.playerLoading=true;},"An attached old cell is insufficient while the player is loading");
    Check(!session.CanBootstrap(ready,1,2),"A session transition invalidates a previously queued startup probe");
    Check(session.CanBootstrap(ready,2,2),"Title-screen coc can initialize without NewGame or PostLoadGame");
    session.Notify(true);
    Check(session.Active() && !session.CanBootstrap(ready,2,2),"Normal initialization runs once, without repeated resets from quiet input polls");
    session.Notify(false);
    Check(!session.Active() && !session.AwaitingWorld() && !session.CanBootstrap(ready,3,3),
        "PreLoadGame must not reactivate the old character before PostLoadGame");
    session.Notify(false);
    Check(!session.CanBootstrap(ready,4,4),"Failed PostLoadGame remains inactive even if old 3D/cell data survives");
    session.Notify(true);
    Check(session.Active(),"A subsequent successful load or normal new game still activates");
    std::cout<<"Game session bootstrap tests passed\n";
}

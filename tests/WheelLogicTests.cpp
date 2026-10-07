#include "WheelLogic.h"
#include "FunctionNavigation.h"
#include "InputGate.h"
#include "OpenPolicy.h"
#include "InputBindings.h"
#include "ActionPolicy.h"
#include "EquipPolicy.h"
#include "NavigationPresentation.h"
#include <cstdlib>
#include <iostream>
void Check(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
int main() {
    using namespace Wheel;
    Check(PageCount(0) == 1 && PageCount(10) == 1 && PageCount(11) == 2 && PageCount(23) == 3, "page boundaries");
    for(int count=1;count<=6;++count)for(int current=0;current<count;++current) {
        const auto ribbon=RibbonLabels(current,count);
        bool centred=false;int seen=0;
        Check(ribbon.size==std::min(count,5),"Ribbon limits category count");
        for(int i=0;i<ribbon.size;++i) {
            const auto label=ribbon.labels[i];
            Check(label.index>=0 && label.index<count && !(seen&(1<<label.index)),"Category labels wrap without duplication");
            seen|=1<<label.index;
            if(label.offset==0)centred=label.index==current;
        }
        Check(centred,"Selected category remains at ribbon centre");
    }
    for(int total: {1,2,5,7,8,100})for(int current=0;current<total;++current) {
        const auto window=VisiblePages(total,current);
        Check(window.size<=7 && window.first>=0 && window.first+window.size<=total && current>=window.first && current<window.first+window.size,"Page rail remains bounded and includes the selected page");
    }
    Check(Wrap(-1, 6) == 5 && Wrap(6, 6) == 0 && Wrap(1, 0) == 0, "wrap");
    Check(HitTest(0, 0) == -1 && HitTest(.1f, .1f) == -1, "neutral centre");
    for (int i = 0; i < slots; ++i) {
        const float a = i * 2 * std::numbers::pi_v<float> / slots;
        Check(HitTest(std::sin(a), -std::cos(a)) == i, "angular slot mapping");
    }
    float x = 0, y = 0;
    MovePointer(x, y, 20, 0);
    Check(std::abs(x - 1) < .001f && y == 0, "pointer bounds");
    MovePointer(x, y, -1, 0);
    Check(HitTest(x, y) == -1, "can return to centre");
    InputGate gate;
    using R = InputGate::Result;
    Check(gate.Filter(17,true,false,false)==R::Pass, "game sees W down");
    Check(gate.Filter(17,true,false,true)==R::Release, "opening wheel releases pre-existing movement");
    Check(gate.Filter(17,true,false,true)==R::Suppress, "movement release occurs only once");
    Check(gate.Filter(256,true,false,true)==R::Suppress, "wheel click is not an attack");
    Check(gate.Filter(256,true,false,false)==R::Suppress, "held wheel click remains suppressed after close");
    Check(gate.Filter(256,false,true,false)==R::Suppress, "wheel click release remains suppressed");
    Check(gate.Filter(256,true,false,false)==R::Pass, "fresh click reaches game normally");
    Check(gate.Filter(17,false,true,false)==R::Suppress, "movement release clears suppression");
    Check(gate.Filter(17,true,false,false)==R::Pass, "fresh movement works after close");
    Check(gate.Filter(264,true,false,true,true)==R::Suppress && !gate.Swallowed(264), "scroll impulses do not stick");
    gate.Filter(30,true,false,true);
    gate.Reset();
    Check(gate.Filter(30,true,false,false)==R::Pass, "focus-loss reset recovers a lost release");
    Check(PlayerEligible(true,true,true,false,false), "normal player and humanoid vampire/custom race eligible without race playable flag");
    Check(!PlayerEligible(true,true,true,false,true), "beast form yields to vanilla");
    Check(!PlayerEligible(false,true,true,false,false) && !PlayerEligible(true,true,true,true,false), "loading and dead player cannot open");
    Check(FavoritesKeyMatches(-1,16,16,"") && FavoritesKeyMatches(-1,33,33,""), "default and rebound physical favorites key");
    Check(!FavoritesKeyMatches(-1,0xFFFFFFFF,33,"Favorites"), "unmapped semantic events are not wheel entry keys");
    Check(!FavoritesKeyMatches(-1,0xFFFFFFFF,16,"") && !FavoritesKeyMatches(-1,255,16,"ToggleFavorite"), "unbound key and inventory ToggleFavorite do not open wheel");
    Check(!FavoritesKeyMatches(44,16,16,"Favorites") && FavoritesKeyMatches(44,16,44,""), "explicit override takes priority");
    for(unsigned key:{82u,79u,80u,81u,2u,3u,4u,5u}) {
        Check(!FavoritesKeyMatches(-1,16,key,"Favorites"),"quick slots cannot impersonate the physical Favorites binding");
        Check(KeyboardOpening(key,0,16,0,16,1)==Opening::None,"quick slots cannot open either default wheel");
        InputGate shortcuts;
        Check(shortcuts.Filter(key,true,false,false)==R::Pass && shortcuts.Filter(key,true,false,false)==R::Pass &&
            shortcuts.Filter(key,false,true,false)==R::Pass,"unrelated shortcuts pass down, hold and up outside the wheel");
    }
    Check(KeyboardOpening(16,0,16,0,16,1)==Opening::Favorites && KeyboardOpening(16,1,16,0,16,1)==Opening::Actions,"default Q and Shift+Q are distinct chords");
    Check(KeyboardOpening(16,3,16,2,16,1)==Opening::None,"extra modifiers do not match another wheel");
    Check(KeyboardOpening(44,6,16,2,44,6)==Opening::Actions && KeyboardOpening(16,2,16,2,44,6)==Opening::Favorites,"separate main keys and Ctrl+Alt action chord");
    Check(KeyboardOpening(16,0,16,0,16,0)==Opening::Actions,"identical bindings use deterministic actions priority");
    Check(GamepadOpening(266,266,true,false)==Opening::Favorites && GamepadOpening(266,266,true,true)==Opening::Actions && GamepadOpening(267,266,true,true)==Opening::None,"controller opening key and modifier priority");
    Check(GamepadOpening(266,-1,true,true)==Opening::None && GamepadOpening(282,282,true,true)==Opening::None,"unmapped controller does not open");
    float sx=.3f,sy=.4f;AimStick(sx,sy,0,1);
    Check(sx==0 && sy==-1,"controller Y axis points up on screen");
    AimStick(sx,sy,.1f,.1f);Check(sx==0 && sy==-1,"releasing stick preserves selection for confirmation");
    Check(StickAxis(.19f)==0 && StickAxis(1)==1 && StickAxis(-1)==-1,"settings pointer deadzone and full range");
    std::cout << "Wheel logic tests passed\n";
    using A = ActionDecision;
    Check(DecideAction(1,1,true,true,false,false,false)==A::Wait, "wait for queued hide");
    Check(DecideAction(1,1,true,false,true,false,false)==A::Wait, "wait for actual menu removal");
    Check(DecideAction(1,1,true,false,false,true,false)==A::Wait, "wait until unpaused");
    Check(DecideAction(1,1,true,false,false,false,false)==A::Submit, "submit only after close and unpause");
    Check(DecideAction(1,2,true,false,false,false,false)==A::Discard, "load invalidates pending use");
    Check(DecideAction(1,1,false,false,false,false,false)==A::Discard, "focus loss, blocking menu or invalid player cancels use");
    Check(DecideAction(1,1,true,false,false,true,true)==A::Discard, "timeout cannot consume later");
    using E=ActionExecutor;
    Check(ExecutorFor(true)==E::PlayerUpdate && ExecutorFor(false)==E::TaskQueue,"face commands use player update; existing actions use task queue");
    Check(DecideActionOn(E::TaskQueue,E::PlayerUpdate,1,1,true,false,false,false,false)==A::Wait,"SKSE task cannot take an unpaused face command");
    Check(DecideActionOn(E::TaskQueue,E::PlayerUpdate,1,2,false,false,false,false,true)==A::Wait,"wrong executor does not consume or cancel another queue's command");
    Check(DecideActionOn(E::PlayerUpdate,E::PlayerUpdate,1,1,true,true,true,true,false)==A::Wait,"player update waits while wheel pauses game");
    Check(DecideActionOn(E::PlayerUpdate,E::PlayerUpdate,1,1,true,false,false,false,false)==A::Submit,"player update submits face command after resume");
    Check(DecideActionOn(E::PlayerUpdate,E::PlayerUpdate,1,2,true,false,false,false,false)==A::Discard,"load invalidates face command");
    Check(DecideActionOn(E::PlayerUpdate,E::PlayerUpdate,1,1,true,false,false,false,true)==A::Discard,"face command timeout cannot replay later");
    Check(DecideActionOn(E::PlayerUpdate,E::TaskQueue,1,1,true,false,false,false,false)==A::Wait,"player update leaves favorite/outfit commands to existing queue");
    using F=FaceLight::Section;
    Check(FaceLight::TypeCount(false)==1 && FaceLight::TypeCount(true)==2,"Optional face provider controls visible type count");
    for(const auto section:{F::Outfits,F::Lighting,F::Followers}) {
        Check(FaceLight::VisibleSection(section,false)==F::Outfits,"Missing provider restores outfit category");
        Check(FaceLight::VisibleSection(section,true)==section,"Installed provider preserves category and child navigation");
        for(int direction:{-1,0,1})Check(FaceLight::ChangeType(section,direction,false)==F::Outfits,"A/D cannot enter missing provider category");
    }
    Check(FaceLight::ChangeType(F::Outfits,1)==F::Lighting && FaceLight::ChangeType(F::Outfits,-1)==F::Lighting,"A/D wraps from outfits to lighting");
    Check(FaceLight::ChangeType(F::Lighting,1)==F::Outfits && FaceLight::ChangeType(F::Lighting,-1)==F::Outfits,"A/D wraps from lighting to outfits");
    Check(FaceLight::ChangeType(F::Followers,1)==F::Outfits && FaceLight::ChangeType(F::Followers,-1)==F::Outfits,"A/D exits child list to other type");
    Check(FaceLight::ChangeType(F::Lighting,0)==F::Lighting,"zero direction does not navigate");
    std::cout << "Deferred action executor and function category tests passed\n";
    Check(UnequipWeapon(false,false,true,false),"Right-hand reselect unequips right instance");
    Check(UnequipWeapon(false,true,false,true),"Left-hand reselect unequips left instance");
    Check(!UnequipWeapon(false,true,true,false),"Opposite hand equips instead of unequipping right");
    Check(!UnequipWeapon(false,false,false,true),"Opposite hand equips instead of unequipping left");
    Check(UnequipWeapon(false,true,true,true) && UnequipWeapon(false,false,true,true),"Dual wield uses selected hand");
    Check(UnequipWeapon(true,true,true,false) && UnequipWeapon(true,false,true,false),"Two-hand weapon toggles from either click");
    Check(!UnequipWeapon(true,false,false,false) && !UnequipWeapon(false,false,false,false),"Unequipped instance never toggles off same-form other instance");
    std::cout<<"Weapon toggle policy passed\n";
}

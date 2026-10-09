#include "WheelLogic.h"
#include "FunctionNavigation.h"
#include "InputGate.h"
#include "ControllerSticks.h"
#include "InputDispatchChain.h"
#include "OpenPolicy.h"
#include "InputBindings.h"
#include "QuickSlots.h"
#include "ActionPolicy.h"
#include "EquipPolicy.h"
#include "NavigationPresentation.h"
#include "MagicCategories.h"
#include <cstdlib>
#include <iostream>
#include <array>
void Check(bool ok, const char* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
void CheckInputDispatch() {
    using namespace Wheel;
    struct Event { unsigned key; bool pressed; Event* next=nullptr; };
    using R=InputGate::Result;
    InputGate gate;
    int voiceActions=0,seenButtons=0;
    // A handler that considers a zero-valued voice button a release would cast
    // the selected lesser power with the former neutralized-event approach.
    auto sink=[&](Event* head) {
        for(auto e=head;e;e=e->next) {
            ++seenButtons;
            if(e->key==275 && !e->pressed)++voiceActions;
        }
    };
    auto dispatch=[&](bool pressed,bool capture) {
        Event rb{275,pressed};
        const auto decision=gate.Filter(275,pressed,!pressed,capture);
        if(decision==R::Suppress || decision==R::Release)rb.pressed=false;
        InputDispatchChain<Event> chain(&rb);
        chain.Append(&rb,decision!=R::Suppress);chain.Finish();sink(*chain.Events());
    };
    dispatch(true,true);dispatch(true,true);dispatch(false,true);
    Check(seenButtons==0 && voiceActions==0,"Wheel RB down/hold/up are absent even for release-driven power handlers");
    dispatch(true,true);dispatch(true,false);dispatch(false,false);
    Check(seenButtons==0 && voiceActions==0,"Closing while holding RB cannot leak a held event or orphan release");
    dispatch(true,false);dispatch(false,false);
    Check(seenButtons==2 && voiceActions==1,"Fresh RB outside the wheel still activates the equipped power");
    seenButtons=voiceActions=0;gate.Reset();
    dispatch(true,false);dispatch(true,true);dispatch(true,true);dispatch(false,true);
    Check(seenButtons==2 && voiceActions==1,"A gameplay button held before opening receives its necessary release exactly once");
    // Mixed batches retain order and the engine's original head/next pointers.
    Event lead{274,true},move{17,true},middle{275,true},text{1000,true},tail{276,true};
    lead.next=&move;move.next=&middle;middle.next=&text;text.next=&tail;
    Event* original=&lead;
    {
        InputDispatchChain<Event> chain(original);
        for(auto e=original;e;e=e->next)chain.Append(e,e==&move || e==&text);
        chain.Finish();
        Check(*chain.Events()==&move && move.next==&text && text.next==nullptr,
            "Leading, middle and trailing captured events are removed without dropping allowed movement or text");
        // Simulate an inner IME hook: inject text, dispatch it synchronously,
        // then restore its own temporary links before returning to the wheel.
        Event injected{2000,true};
        text.next=&injected;
        auto cursor=*chain.Events();int seen=0;
        for(;cursor;cursor=cursor->next)++seen;
        Check(seen==3,"Text injected by an inner hook reaches the same downstream dispatch");
        text.next=nullptr;
    }
    Check(original==&lead && lead.next==&move && move.next==&middle && middle.next==&text && text.next==&tail && !tail.next,
        "Original engine queue is restored after nested input hooks");
    // The inverse hook order places the injected character in our input chain.
    Event injected{2000,true};tail.next=&injected;
    {
        InputDispatchChain<Event> chain(original);
        for(auto e=original;e;e=e->next)chain.Append(e,e==&move || e==&text || e==&injected);
        chain.Finish();
        Check(move.next==&text && text.next==&injected && !injected.next,
            "Text already injected by an outer hook remains visible");
    }
    Check(text.next==&tail && tail.next==&injected,"Outer-hook injected queue links are also restored");
    tail.next=nullptr;
    try {
        InputDispatchChain<Event> chain(original);
        for(auto e=original;e;e=e->next)chain.Append(e,e==&move);
        chain.Finish();throw 1;
    } catch(int) {}
    Check(move.next==&middle && text.next==&tail,"Exception unwinding restores engine-owned links");
    {
        InputDispatchChain<Event> chain(original);
        for(auto e=original;e;e=e->next)chain.Append(e,false);
        chain.Finish();Check(!*chain.Events(),"Fully captured batches still dispatch an empty chain");
    }
    Check(lead.next==&move && move.next==&middle,"Fully captured batches leave the engine queue intact");
    {
        InputDispatchChain<Event> chain(original);
        for(auto e=original;e;e=e->next)chain.Append(e,true);
        chain.Finish();Check(*chain.Events()==original && text.next==&tail,"Idle dispatch keeps every event intact");
    }
    InputDispatchChain<Event> empty(nullptr);empty.Finish();Check(!*empty.Events(),"Empty input polls remain valid");
}
int main() {
    CheckInputDispatch();
    using namespace Wheel;
    Check(PageCount(0) == 1 && PageCount(10) == 1 && PageCount(11) == 2 && PageCount(23) == 3, "page boundaries");
    for(int count=1;count<=categoryCount;++count)for(int current=0;current<count;++current) {
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
    {
        enum class Type {kSpell,kPower,kLesserPower,kVoicePower,kAbility,kDisease,kEnchantment,kScroll};
        Check(categoryCount==8,"Spells, shouts and powers are independent top-level categories");
        Check(MagicCategory(false,Type::kSpell)==Category::Spells,"Ordinary spells are separated from voice-slot actions");
        for(auto type:{Type::kPower,Type::kLesserPower,Type::kVoicePower})
            Check(MagicCategory(false,type)==Category::Powers,"Active greater, lesser and voice powers share the powers category");
        for(auto type:{Type::kAbility,Type::kDisease,Type::kEnchantment,Type::kScroll})
            Check(!MagicCategory(false,type),"Passive/internal magic types are never equip actions");
        Check(MagicCategory(true,Type::kVoicePower)==Category::Shouts,"A shout record takes precedence over its spell-type placeholder");
        Check(Wrap(static_cast<int>(Category::Spells)+1,categoryCount)==static_cast<int>(Category::Shouts) &&
            Wrap(static_cast<int>(Category::Shouts)+1,categoryCount)==static_cast<int>(Category::Powers),"Magic categories remain adjacent in navigation");
    }
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
    {
        InputGate movement;
        Check(IsMovementEvent("Forward") && IsMovementEvent("Back") && IsMovementEvent("Strafe Left") && IsMovementEvent("Strafe Right"),
            "Only directional gameplay bindings identify movement regardless of the physical key");
        Check(!IsMovementEvent("Left Attack/Block") && !IsMovementEvent("Jump") && !IsMovementEvent("Favorites") && !IsMovementEvent("Sprint"),
            "Combat, entrances and toggle actions cannot acquire movement passthrough");
        Check(movement.Filter(17,true,false,false,false,true)==R::Pass,"Movement starts in gameplay");
        Check(movement.Filter(17,true,false,true,false,true)==R::Pass &&
            movement.Filter(17,true,false,true,false,true)==R::Pass,"Opening preserves an already held direction without synthesizing release");
        Check(movement.Filter(30,true,false,true,false,true)==R::Suppress &&
            movement.Filter(30,true,false,true,false,true)==R::Suppress,"New wheel direction presses cannot change character direction");
        Check(movement.Filter(17,false,true,true,false,true)==R::Pass,"Releasing preexisting movement while open reaches gameplay immediately");
        Check(movement.Filter(17,true,false,true,false,true)==R::Suppress,"Re-pressing a released movement key only operates the wheel");
        Check(movement.Filter(17,true,false,false,false,true)==R::Resume,"A UI-held W resumes immediately after actual close without physical re-press");
        Check(movement.Filter(17,true,false,false,false,true)==R::Pass,"Resumed held movement does not emit repeated new presses");
        Check(movement.Filter(17,false,true,false,false,true)==R::Pass,"Release after resume stops movement normally");
        Check(movement.Filter(30,false,true,false,false,true)==R::Suppress,"A new UI movement key released at close emits no orphan gameplay release");
        movement.Filter(17,true,false,false,false,true);
        movement.Filter(17,true,false,true,false,true);
        Check(movement.Filter(17,true,false,false,false,true)==R::Resume,"Preexisting held direction also re-establishes input after a paused dialog");
        movement.Filter(256,true,false,true);
        Check(movement.Filter(256,true,false,false)==R::Suppress && movement.Filter(256,false,true,false)==R::Suppress,
            "Movement recovery cannot release a UI click into an attack");
        movement.Filter(16,true,false,true);
        Check(movement.Filter(16,true,false,false)==R::Suppress,"Wheel entrance remains swallowed until physical release");
        movement.Reset();
        Check(movement.Filter(17,true,false,true,false,true)==R::Suppress,"Focus/load reset cannot retain a stale allowed direction");
        MovementStickGate stick;
        auto filter=[&](float x,float y){stick.Filter(x,y);return std::array<float,2>{x,y};};
        Check(filter(.3f,.8f)==std::array<float,2>{.3f,.8f},"Gameplay left stick passes normally");
        stick.Capture(true);
        Check(filter(-.9f,.1f)==std::array<float,2>{.3f,.8f},"Wheel aiming keeps the opening movement vector, not the new aim");
        Check(filter(.05f,.1f)==std::array<float,2>{0,0},"Centering the stick stops the retained movement");
        Check(filter(1,0)==std::array<float,2>{0,0},"New stick aim cannot restart movement inside the wheel");
        stick.Capture(false);
        Check(filter(1,0)==std::array<float,2>{1,0},"Closing immediately returns the current stick position to gameplay");
        stick.Reset();stick.Capture(true);
        Check(filter(0,1)==std::array<float,2>{0,0},"Idle or reset stick never starts movement on opening");
        using S=LeftStickMode;
        for(bool split:{false,true})for(bool capture:{false,true})for(bool open:{false,true})
            for(bool modal:{false,true})for(int time=0;time<3;++time)for(bool transition:{false,true}) {
                const auto mode=ControllerLeftStick(split,capture,open,modal,time,transition);
                if(!capture)Check(mode==S::Gameplay,"Gameplay always receives the physical left stick outside capture");
                else if(!split)Check(mode==S::HeldDirection,"Legacy selection keeps opening-vector behavior in every wheel state");
                else Check(mode==((open || transition) && !modal && time!=0?S::Gameplay:S::Block),
                    "Split movement is available only in a live wheel lifecycle with no blocking modal");
            }
        Check(ControllerSelectionStick(false,true,false) && !ControllerSelectionStick(false,false,true) &&
            ControllerSelectionStick(true,false,true) && !ControllerSelectionStick(true,true,false),
            "Exactly one physical stick drives selection in each scheme");
        // Simulate opening from rest, changing direction, modal stop and resuming
        // physical movement. Neither a shared gate nor centering is required.
        MovementStickGate splitMovement;
        auto route=[&](float x,float y,bool modal,bool capture=true){
            const auto mode=ControllerLeftStick(true,capture,true,modal,1,false);
            splitMovement.Capture(mode==S::HeldDirection);
            if(mode==S::Block)x=y=0;
            splitMovement.Filter(x,y);return std::array<float,2>{x,y};
        };
        Check(route(0,0,false)==std::array<float,2>{0,0} && route(.8f,.1f,false)==std::array<float,2>{.8f,.1f} &&
            route(-.3f,.9f,false)==std::array<float,2>{-.3f,.9f},"Split mode starts and redirects movement while the wheel is open");
        Check(route(.8f,.1f,true)==std::array<float,2>{0,0} && route(.8f,.1f,false)==std::array<float,2>{.8f,.1f},
            "Modal dialogs stop movement and returning to the live wheel resumes the physical stick");
        Check(route(.8f,.1f,false,false)==std::array<float,2>{.8f,.1f},"Closing does not stop split left-stick movement");
        LookStickGate look;
        auto camera=[&](float x,float y,bool capture,bool protect){look.Filter(x,y,capture,protect);return std::array<float,2>{x,y};};
        Check(camera(.6f,.4f,false,false)==std::array<float,2>{.6f,.4f},"Legacy camera input passes normally outside the wheel");
        Check(camera(.6f,.4f,true,true)==std::array<float,2>{0,0},"Right-stick UI selection never turns the camera");
        Check(camera(-.4f,.7f,false,false)==std::array<float,2>{0,0},"Closing or disabling split mode with right stick held cannot snap the camera");
        Check(camera(.05f,.05f,false,false)==std::array<float,2>{.05f,.05f} &&
            camera(-.4f,.7f,false,false)==std::array<float,2>{-.4f,.7f},"Returning to center restores ordinary camera control");
        look.Reset(true);
        Check(camera(.9f,0,false,true)==std::array<float,2>{0,0},"Focus/load/disconnect reset blocks a stale right-stick deflection");
        camera(0,0,false,true);
        Check(camera(.9f,0,false,true)==std::array<float,2>{.9f,0},"After reset centering restores camera input");
        look.Reset();camera(.9f,0,true,false);
        Check(camera(.9f,0,false,false)==std::array<float,2>{.9f,0},"Opt-out retains the original camera handoff");
    }
    Check(PlayerEligible(true,true,true,false,false), "normal player and humanoid vampire/custom race eligible without race playable flag");
    Check(!PlayerEligible(true,true,true,false,true), "beast form yields to vanilla");
    Check(!PlayerEligible(false,true,true,false,false) && !PlayerEligible(true,true,true,true,false), "loading and dead player cannot open");
    Check(FavoritesKeyMatches(-1,16,16,"") && FavoritesKeyMatches(-1,33,33,""), "default and rebound physical favorites key");
    Check(ResolveFavoritesKey(-1,51)==51 && !FavoritesKeyMatches(-1,51,16,""),"Reported mapped=51 follows comma, not physical Q");
    Check(KeyboardOpening(51,0,51,0,51,1)==Opening::Favorites && KeyboardOpening(51,1,51,0,51,1)==Opening::Actions,
        "Both default wheel chords follow the game's remapped Favorites key");
    Check(KeyboardOpening(16,0,ResolveFavoritesKey(16,51),0,16,1)==Opening::Favorites &&
        KeyboardOpening(16,1,16,0,16,1)==Opening::Actions,"Explicit Hotkey=16 restores Q and Shift+Q without altering game controls");
    {
        constexpr std::uint32_t menu=1u<<3,contextual=1u<<13,invalid=1u<<31;
        Check(EntryControlGroupEnabled(menu,menu) && EntryControlGroupEnabled(0,0),"Ordinary and ungrouped entrances remain available");
        Check(EntryControlGroupEnabled(invalid,0) && EntryControlGroupEnabled(0xFFFFFFFF,0),"Invalid group sentinel is not a disabled control");
        Check(!EntryControlGroupEnabled(menu|contextual,menu),"A contextual mod can disable Favorites without disabling menu controls");
        Check(EntryControlGroupEnabled(menu|contextual,menu|contextual),"Leaving the looting context re-enables the same Favorites mapping");
        Check(!EntryControlGroupEnabled(menu,contextual),"Native disabled control groups also retain priority");
        InputGate contextualGate;
        using Result=InputGate::Result;
        for(bool pressed:{true,true,false}) {
            const bool shouldCapture=EntryControlGroupEnabled(menu|contextual,menu);
            Check(contextualGate.Filter(16,pressed,!pressed,shouldCapture)==Result::Pass,
                "Yielded Q down/hold/up reach a contextual looting handler unchanged");
        }
        Check(contextualGate.Filter(16,true,false,EntryControlGroupEnabled(menu|contextual,menu|contextual))==Result::Suppress,
            "A fresh enabled entrance can open and capture normally after looting");
        Check(contextualGate.Filter(16,true,false,true)==Result::Suppress && contextualGate.Filter(16,false,true,true)==Result::Suppress,
            "An already captured entrance remains protected if control groups change while the wheel is open");
    }
    Check(!FavoritesKeyMatches(-1,0xFFFFFFFF,33,"Favorites"), "unmapped semantic events are not wheel entry keys");
    Check(!FavoritesKeyMatches(-1,0xFFFFFFFF,16,"") && !FavoritesKeyMatches(-1,255,16,"ToggleFavorite"), "unbound key and inventory ToggleFavorite do not open wheel");
    Check(!FavoritesKeyMatches(44,16,16,"Favorites") && FavoritesKeyMatches(44,16,44,""), "explicit override takes priority");
    for(unsigned key:{82u,79u,80u,81u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u}) {
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
    {
        ControllerPageButtons buttons;
        InputGate capture;
        int page=0;
        auto event=[&](unsigned key,bool pressed,float duration,bool open) {
            const auto identity=(std::uint64_t{2}<<32)|key;
            const bool nativeDown=pressed && duration==0;
            const bool edge=buttons.Observe(key,pressed,nativeDown);
            if(open && edge && !capture.Swallowed(identity))page=Wrap(page+(key==266?-1:1),3);
            capture.Filter(identity,pressed,!pressed,open);
        };
        event(266,true,0,false); // Opening key observed before the wheel opens.
        event(266,true,.016f,true);
        Check(page==0,"Held controller entrance cannot also turn the wheel page");
        event(266,false,0,true); // Native IsUp is false, but physically released.
        event(266,true,.016f,true); // First press need not have zero duration.
        Check(page==2,"D-pad up wraps from first to last page with a nonzero-duration first press");
        event(266,true,0,true);event(266,true,.1f,true);
        Check(page==2,"Repeated or zero-duration held events never turn multiple pages");
        event(266,false,0,true);event(267,true,.02f,true);
        Check(page==0,"D-pad down wraps to the first page and works on its first observed press");
        event(267,false,0,true);event(267,true,0,true);
        Check(page==1,"A zero-duration release clears swallowing for the next controller page press");
        event(267,false,.1f,true);event(267,true,0,true);
        Check(page==2,"Normal duration-based controller events keep working");
        buttons.Reset();
        Check(!buttons.Observe(266,true,false),"Reset cannot turn an already held controller button into paging");
        Check(!buttons.Observe(266,false,false)&&buttons.Observe(266,true,false),"Releasing after reset re-arms a nonzero-duration first press");
        buttons.Reset();
        Check(buttons.Observe(267,true,true)&&!buttons.Observe(267,true,true),"A fresh native down after reset pages once even if zero duration repeats");
        Check(!buttons.Observe(274,true,true),"Category buttons do not become page events");
    }
    float sx=.3f,sy=.4f;AimStick(sx,sy,0,1);
    Check(sx==0 && sy==-1,"controller Y axis points up on screen");
    AimStick(sx,sy,.1f,.1f);Check(sx==0 && sy==-1,"releasing stick preserves selection for confirmation");
    Check(StickAxis(.19f)==0 && StickAxis(1)==1 && StickAxis(-1)==-1,"settings pointer deadzone and full range");
    for(unsigned key=2;key<=9;++key)Check(QuickSlotFromKey(key,"")==static_cast<int>(key)-2,"1..8 map to native quick slots, not radial sectors");
    Check(QuickSlotFromKey(10,"")==-1 && QuickSlotFromKey(11,"")==-1 && QuickSlotFromKey(82,"")==-1,"9/0 and unmapped Numpad are not invented native slots");
    Check(QuickSlotFromKey(79,"Hotkey1")==0 && QuickSlotFromKey(80,"Hotkey8")==7 && QuickSlotFromKey(2,"Hotkey3")==2,"explicit game slot remaps take priority");
    Check(QuickSlotFromKey(79,"Hotkey9")==-1 && QuickSlotFromKey(81,"Hotkey10")==-1,"extension events are not coerced to native slots");
    Check(QuickSlotFromKey(2,"Hotkey9")==-1 && QuickSlotFromKey(3,"Hotkey10")==-1,"extension slot remaps override the physical number fallback");
    Check(AssignedQuickSlot(-1,3)==3 && AssignedQuickSlot(2,3)==3 && AssignedQuickSlot(3,3)==-1,"assign, move and toggle the same native slot");
    for(int invalid:{-1,8,255})Check(ReassignedQuickSlot(2,true,invalid,-1)==2,"invalid slot request preserves existing data");
    // A single native slot namespace spans exact armor/weapon instances and spells.
    // Replacing armor with a spell clears just that slot; moving/toggling the
    // selected item leaves every unrelated slot and extension slot intact.
    std::array<int,5> assignments{0,3,7,-1,9};
    auto assign=[&](int selected,int requested){const int next=AssignedQuickSlot(assignments[selected],requested);
        for(int i=0;i<static_cast<int>(assignments.size());++i)assignments[i]=ReassignedQuickSlot(assignments[i],i==selected,requested,next);};
    assign(3,3);Check(assignments==std::array<int,5>{0,-1,7,3,9},"spell replacement clears the old item in the requested slot");
    assign(3,7);Check(assignments==std::array<int,5>{0,-1,-1,7,9},"moving a spell releases its old slot and the new slot occupant");
    assign(1,7);Check(assignments==std::array<int,5>{0,7,-1,-1,9},"item replacement clears the old spell without touching extensions");
    assign(1,7);Check(assignments==std::array<int,5>{0,-1,-1,-1,9},"same-key toggle keeps unrelated native and extension bindings");
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
    Check(KeepWheelAfterUse(true,false) && !KeepWheelAfterUse(true,true) && !KeepWheelAfterUse(false,false) && !KeepWheelAfterUse(false,true),"Equipment can stay open; consumables and default mode always close");
    Check(DecideRetainedActionOn(E::TaskQueue,E::TaskQueue,1,1,true,false,true,false,false,true,true,true)==A::Submit,"Retained action executes without removing its own overlay");
    Check(DecideRetainedActionOn(E::TaskQueue,E::TaskQueue,1,1,true,false,true,true,false,true,true,true)==A::Wait,"Retained action waits for actual unpause");
    Check(DecideRetainedActionOn(E::TaskQueue,E::TaskQueue,1,1,true,false,true,false,false,false,true,true)==A::Wait,"Default action still waits for menu removal");
    Check(DecideRetainedActionOn(E::TaskQueue,E::TaskQueue,1,1,true,false,true,false,false,true,false,true)==A::Discard,"Retained action never lands in a newer wheel");
    Check(DecideRetainedActionOn(E::TaskQueue,E::TaskQueue,1,1,true,false,false,false,false,true,false,false)==A::Submit,"Manual close still allows the already accepted action");
    Check(DecideRetainedActionOn(E::TaskQueue,E::TaskQueue,1,2,true,false,true,false,false,true,true,true)==A::Discard,"Load cancels retained action");
    Check(DecideRetainedActionOn(E::TaskQueue,E::PlayerUpdate,1,1,true,false,true,false,false,true,true,true)==A::Wait,"Retained face command keeps its player-update owner");
    Check(!RetainedActionSettled(true,false,1,10) && !RetainedActionSettled(false,true,1,10),"Pending commands and outfit jobs keep game-thread window open");
    Check(!RetainedActionSettled(false,false,.1,10) && !RetainedActionSettled(false,false,1,1) && RetainedActionSettled(false,false,.15,2),"Retained window needs wall time and two native updates");
    using P=PadWheelCommand;
    for(int scheme:{0,1}) {
        const unsigned leftCategory=scheme?280:274,rightCategory=leftCategory+1,leftUse=scheme?274:280,rightUse=leftUse+1;
        Check(ControllerCommand(leftCategory,scheme)==P::PreviousCategory && ControllerCommand(rightCategory,scheme)==P::NextCategory,"Selected pair changes categories");
        Check(ControllerCommand(leftUse,scheme)==P::UseLeft && ControllerCommand(rightUse,scheme)==P::UseRight,"Other pair equips left/right with no category conflict");
        Check(ControllerCommand(276,scheme)==P::UseRight && ControllerCommand(278,scheme)==P::UseLeft && ControllerCommand(277,scheme)==P::Back,"A/X/B retain behavior in both schemes");
        Check(ControllerCommand(266,scheme)==P::PreviousPage && ControllerCommand(267,scheme)==P::NextPage,"D-pad pages remain independent of category scheme");
    }
    ControllerPageButtons triggers{280};
    Check(triggers.Observe(281,true,false) && !triggers.Observe(281,true,true),"Trigger first press works without native down and holding never repeats");
    Check(!triggers.Observe(281,false,false) && triggers.Observe(281,true,false),"Trigger release re-arms use");
    triggers.Reset();Check(!triggers.Observe(280,true,false),"Reset does not use an already held trigger");
    for(unsigned key:{274u,275u,280u,281u}) {
        InputGate triggerGate;
        Check(triggerGate.Filter(key,true,false,true)==InputGate::Result::Suppress &&
            triggerGate.Filter(key,true,false,false)==InputGate::Result::Suppress &&
            triggerGate.Filter(key,false,true,false)==InputGate::Result::Suppress,"Use/category pairs remain captured through release after closing");
        Check(triggerGate.Filter(key,true,false,false)==InputGate::Result::Pass,"Fresh trigger/bumpers pass normally outside the wheel");
    }
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

#include "TimePolicy.h"
#include "QueuedTask.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
namespace {
    void Check(bool value,const char* message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
    bool Matches(Wheel::TimePair a,Wheel::TimePair b){return Wheel::SameTime(a.current,b.current)&&Wheel::SameTime(a.target,b.target);}
    void TaskFailures() {
        using namespace Wheel;
        TimeLease lease;
        TimePair engine=*lease.Begin({.5f,.5f},.2f);
        std::atomic<bool> queued{true};
        std::function<void()> callback;
        int errors=0,runs=0;
        std::string message;
        auto failure=[&](const char* error) {
            ++errors;message=error;
            if(auto restore=lease.End(engine))engine=*restore;
        };
        ScheduleQueuedTask(queued,[&] {
            callback=[&] {RunQueuedTask(queued,[] {throw std::runtime_error("inventory read failed");},failure);};
        },failure);
        Check(queued && errors==0 && lease.Active(),"Queued work retains its slot and slowdown until execution");
        callback();
        Check(!queued && errors==1 && message=="inventory read failed" && !lease.Active() && Matches(engine,{.5f,.5f}),
            "A failed inventory task frees its slot and error cleanup restores the preexisting time effect");
        Check(!queued.exchange(true),"A later request can reuse the failed queue slot");
        RunQueuedTask(queued,[&] {++runs;},failure);
        Check(!queued && runs==1 && errors==1,"Successful retry runs once without repeating error cleanup");
        queued=true;
        RunQueuedTask(queued,[] {return;},failure);
        Check(!queued && errors==1,"An obsolete task returning early still frees its queue slot");
        engine=*lease.Begin({.5f,.5f},.2f);queued=true;
        ScheduleQueuedTask(queued,[] {throw std::bad_alloc();},failure);
        Check(!queued && errors==2 && !lease.Active() && Matches(engine,{.5f,.5f}),
            "Allocation failure while scheduling cannot permanently block reopening or leave slowdown owned");
        queued=true;
        RunQueuedTask(queued,[] {throw 7;},failure);
        Check(!queued && errors==3 && !message.empty(),"An unknown C++ exception releases the slot and reports once");
        queued=true;bool propagated=false;
        try {RunQueuedTask(queued,[] {throw std::runtime_error("read failed");},[](const char*) {throw std::runtime_error("report failed");});}
        catch(const std::runtime_error&){propagated=true;}
        Check(propagated && !queued,"Unwinding the error handler still releases the slot");
    }
}
int main() {
    using namespace Wheel;
    for(int mode=0;mode<3;++mode) {
        Check(PauseForWheel(mode,false,0)==(mode==0),"Only pause mode pauses the ordinary wheel");
        Check(PauseForWheel(mode,true,0),"All settings pages pause independently of the wheel mode");
        for(int dialog=1;dialog<=5;++dialog)Check(PauseForWheel(mode,false,dialog),"Name/manage/confirmation dialogs always pause");
    }
    TimeLease lease;
    auto applied=lease.Begin({1,1},.2f);Check(applied && Matches(*applied,{.2f,.2f}),"Slowdown is relative to the baseline");
    Check(!lease.Begin({1,1},.2f),"A second opening cannot compound an active lease");
    for(int i=0;i<200;++i)Check(lease.Observe(*applied),"Stable time is observed without rewriting or drift");
    auto restored=lease.End(*applied);Check(restored && Matches(*restored,{1,1}),"Normal close restores our own multiplier");
    Check(!lease.End({1,1}),"Repeated close/forced-hide cleanup does not restore twice");
    applied=lease.Begin({.5f,.5f},.2f);Check(applied && Matches(*applied,{.1f,.1f}),"An existing slow-time effect is never accelerated");
    restored=lease.End(*applied);Check(restored && Matches(*restored,{.5f,.5f}),"Existing slow time survives close instead of restoring hard-coded 1");
    applied=lease.Begin({1,.5f},.2f);
    Check(applied && Matches(*applied,{.2f,.1f}) && lease.Observe({.18f,.1f}) && lease.Observe({.15f,.1f}),"Native current-to-target interpolation remains owned");
    restored=lease.End({.15f,.1f});Check(restored && Matches(*restored,{.75f,.5f}),"Release preserves transition progress and its original target");
    applied=lease.Begin({.5f,1},.2f);Check(applied && lease.Observe({.15f,.2f}),"Increasing native transitions remain owned");
    restored=lease.End({.15f,.2f});Check(restored && Matches(*restored,{.75f,1}),"Increasing transition restores relative progress");
    applied=lease.Begin({1,1},.2f);restored=lease.End(*applied);Check(restored && Matches(*restored,{1,1}),"Entering a dialog releases slowdown before the pause guard takes over");
    applied=lease.Begin({.4f,.4f},.3f);Check(applied && Matches(*applied,{.12f,.12f}),"Leaving a paused dialog acquires the latest external baseline and applied setting");
    restored=lease.End(*applied);Check(restored && Matches(*restored,{.4f,.4f}),"Subsequent cancellation preserves that baseline");
    applied=lease.Begin({1,1},.2f);Check(!lease.Observe({.2f,.3f}) && !lease.Active(),"A changed external target relinquishes ownership");
    Check(!lease.End({.2f,.3f}),"Foreign time is never overwritten by cleanup");
    applied=lease.Begin({1,1},.2f);Check(!lease.Observe({.4f,.2f}),"A changed external current relinquishes ownership even with the same target");
    applied=lease.Begin({1,.5f},.2f);Check(lease.Observe({.15f,.1f}) && !lease.Observe({.18f,.1f}),"Backwards external motion is not mistaken for native interpolation");
    Check(!lease.End({.18f,.1f}),"A canceled lease cannot restore an old snapshot");
    for(auto invalid:{TimePair{0,1},TimePair{-1,1},TimePair{1,0},TimePair{std::numeric_limits<float>::infinity(),1},TimePair{1,std::numeric_limits<float>::quiet_NaN()}})
        Check(!lease.Begin(invalid,.2f),"Invalid engine multipliers are left unchanged");
    for(float invalid:{0.f,-1.f,.01f,2.f,std::numeric_limits<float>::quiet_NaN()})Check(!lease.Begin({1,1},invalid),"Invalid factors do not acquire time");
    for(float factor:{.05f,.1f,.2f,.3f,1.f})for(int i=0;i<100;++i) {
        applied=lease.Begin({.75f,.75f},factor);restored=lease.End(*applied);
        Check(restored && Matches(*restored,{.75f,.75f}),"Rapid open/close cycles never compound the slowdown");
    }
    TaskFailures();
    std::cout<<"Relative slowdown, native transitions, nested pause, external ownership, repeated cleanup and task failure recovery passed\n";
}

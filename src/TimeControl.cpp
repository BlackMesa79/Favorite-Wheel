#include "TimeControl.h"
#include "TimePolicy.h"

namespace Wheel::TimeControl {
    namespace {
        std::mutex mutex;
        TimeLease lease;
        bool session=false,conflict=false;
        int appliedPercent=20;
        TimePair Read() {return {RE::BSTimer::QGlobalTimeMultiplier(),RE::BSTimer::QGlobalTimeMultiplierTarget()};}
        void Write(TimePair value) {
            // Native setter maintains engine bookkeeping. Set the declared current
            // and target globals separately to preserve an in-progress baseline
            // transition and to avoid a delayed UI slowdown/restoration with
            // bChangeTimeMultSlowly. Never change the timer's pauseCount or INI flag.
            RE::BSTimer::GetSingleton()->SetGlobalTimeMultiplier(value.target,true);
            static REL::Relocation<float*> current{RELOCATION_ID(511882,388442)};
            static REL::Relocation<float*> target{RELOCATION_ID(511883,388443)};
            *current=value.current;*target=value.target;
        }
        void Release() {
            if(!lease.Active())return;
            if(auto restore=lease.End(Read())) {
                Write(*restore);
                SKSE::log::info("Wheel time restored: current={} target={}",restore->current,restore->target);
            }else {
                conflict=true;
                SKSE::log::warn("Wheel time ownership changed externally; preserving current multipliers");
            }
        }
    }
    void BeginSession() {
        std::lock_guard lock(mutex);
        Release();session=true;conflict=false;
    }
    bool Update(bool slow,int percent) {
        std::lock_guard lock(mutex);
        if(!session)return true;
        slow=slow && percent<100; // A 100% choice needs no ownership or native write.
        if(lease.Active() && !lease.Observe(Read())) {
            conflict=true;
            SKSE::log::warn("Wheel time ownership changed externally; closing wheel without rewriting time");
        }
        if(conflict)return false;
        if(!slow || percent!=appliedPercent)Release();
        if(conflict)return false;
        if(slow && !lease.Active()) {
            auto timer=RE::BSTimer::GetSingleton();if(!timer)return false;
            const auto base=Read();const auto next=lease.Begin(base,std::clamp(percent,5,100)/100.f);
            if(!next)return false;
            Write(*next);appliedPercent=percent;
            SKSE::log::info("Wheel time applied: percent={} baseline={}/{} applied={}/{}",percent,base.current,base.target,next->current,next->target);
        }
        return true;
    }
    void EndSession() {std::lock_guard lock(mutex);Release();session=false;conflict=false;}
    bool Active() {std::lock_guard lock(mutex);return lease.Active();}
}

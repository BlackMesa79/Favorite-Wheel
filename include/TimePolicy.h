#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace Wheel {
    struct TimePair {
        float current=1,target=1;
        bool operator==(const TimePair&)const=default;
    };
    inline bool ValidTime(TimePair value) {
        return std::isfinite(value.current) && std::isfinite(value.target) && value.current>0 && value.target>0;
    }
    inline bool SameTime(float a,float b) {
        return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<=std::max(1e-6f,std::max(std::abs(a),std::abs(b))*1e-5f);
    }
    // A lease only restores its own write. The engine may interpolate current
    // toward target, but a new target or a backwards/out-of-range current yields
    // ownership. Never divide an unknown external write by our slowdown factor.
    class TimeLease {
        TimePair before{},applied{};
        float lastCurrent=1;
        bool active=false;
    public:
        bool Active()const{return active;}
        std::optional<TimePair> Begin(TimePair value,float factor) {
            if(active || !ValidTime(value) || !std::isfinite(factor) || factor<.05f || factor>1)return {};
            before=value;applied={value.current*factor,value.target*factor};
            if(!ValidTime(applied))return {};
            active=true;lastCurrent=applied.current;return applied;
        }
        bool Observe(TimePair now) {
            if(!active)return true;
            const float low=std::min(lastCurrent,applied.target),high=std::max(lastCurrent,applied.target);
            if(!ValidTime(now) || !SameTime(now.target,applied.target) ||
                (now.current<low && !SameTime(now.current,low)) || (now.current>high && !SameTime(now.current,high))) {
                active=false;return false;
            }
            lastCurrent=now.current;return true;
        }
        std::optional<TimePair> End(TimePair now) {
            if(!active || !Observe(now))return {};
            active=false;
            // Preserve progress of an existing native transition rather than
            // resetting it to the value captured at the start of the wheel.
            return TimePair{now.current*(before.current/applied.current),before.target};
        }
    };
    inline bool PauseForWheel(int mode,bool settings,int dialog){return mode==0 || settings || dialog!=0;}
}

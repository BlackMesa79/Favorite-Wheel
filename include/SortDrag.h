#pragma once
#include <cmath>
namespace Wheel {
    struct SortDrag {
        bool held=false,active=false;
        float startX=0,startY=0;
        void Begin(float x,float y){held=true;active=false;startX=x;startY=y;}
        void Move(float x,float y){if(held && std::hypot(x-startX,y-startY)>=.05f)active=true;}
        void Page(){if(held)active=true;}
        void Cancel(){held=active=false;}
    };
}

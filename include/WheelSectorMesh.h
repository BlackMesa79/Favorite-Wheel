#pragma once
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace Wheel {
    // The contour is outer begin->end, then inner end->begin. Its known
    // topology lets us connect corresponding arc points in O(n), avoiding
    // ImGui's general concave-polygon ear clipping on every animation frame.
    inline void FillSectorStrip(ImDrawList* d, const ImVec2* points, int arcPoints, ImU32 color) {
        if(arcPoints<2 || !(color & IM_COL32_A_MASK))return;
        const int count=arcPoints*2;
        const bool aa=(d->Flags & ImDrawListFlags_AntiAliasedFill)!=0;
        const int stride=aa?2:1;
        d->PrimReserve((arcPoints-1)*6+(aa?count*6:0),count*stride);
        // PrimReserve can change the base index when 16-bit indices overflow.
        const unsigned base=d->_VtxCurrentIdx;
        const auto uv=d->_Data->TexUvWhitePixel;
        for(int i=0;i<arcPoints-1;++i) {
            const unsigned outer0=base+i*stride,outer1=outer0+stride;
            const unsigned inner0=base+(count-1-i)*stride,inner1=inner0-stride;
            for(const auto index:{outer0,outer1,inner1,outer0,inner1,inner0})
                *d->_IdxWritePtr++=static_cast<ImDrawIdx>(index);
        }
        if(aa) {
            // Match ImGui's boundary fringe, including radial edges. Internal
            // quad diagonals have no AA fringe, so there are no seams/overdraw.
            d->_Data->TempBuffer.reserve_discard(count);
            auto normals=d->_Data->TempBuffer.Data;
            for(int prev=count-1,i=0;i<count;prev=i++) {
                float dx=points[i].x-points[prev].x,dy=points[i].y-points[prev].y;
                const float length2=dx*dx+dy*dy;
                if(length2>0) {const float inverse=ImRsqrt(length2);dx*=inverse;dy*=inverse;}
                normals[prev]={dy,-dx};
            }
            for(int prev=count-1,i=0;i<count;prev=i++) {
                float dx=(normals[prev].x+normals[i].x)*.5f,dy=(normals[prev].y+normals[i].y)*.5f;
                const float length2=dx*dx+dy*dy;
                if(length2>.000001f) {const float inverse=std::min(1.f/length2,100.f);dx*=inverse;dy*=inverse;}
                dx*=d->_FringeScale*.5f;dy*=d->_FringeScale*.5f;
                *d->_VtxWritePtr++={{points[i].x-dx,points[i].y-dy},uv,color};
                *d->_VtxWritePtr++={{points[i].x+dx,points[i].y+dy},uv,color & ~IM_COL32_A_MASK};
                const unsigned a=base+i*2,b=base+prev*2;
                for(const auto index:{a,b,b+1,b+1,a+1,a})
                    *d->_IdxWritePtr++=static_cast<ImDrawIdx>(index);
            }
        } else {
            for(int i=0;i<count;++i)*d->_VtxWritePtr++={points[i],uv,color};
        }
        d->_VtxCurrentIdx+=count*stride;
    }
}

#include "WheelSectorMesh.h"
#include "LayoutSkin.h"
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>

void Require(bool condition) {if(!condition)throw std::runtime_error("Sector mesh regression");}
double Area(const ImDrawList& list,int triangle) {
    const auto& a=list.VtxBuffer[list.IdxBuffer[triangle]].pos;
    const auto& b=list.VtxBuffer[list.IdxBuffer[triangle+1]].pos;
    const auto& c=list.VtxBuffer[list.IdxBuffer[triangle+2]].pos;
    return ((double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x))*.5;
}
int main() {
    ImGui::CreateContext();
    auto shared=ImGui::GetDrawListSharedData();
    constexpr ImU32 color=IM_COL32(100,120,140,255);
    int cases=0;
    for(const auto shape:{Wheel::WheelShape::Circle,Wheel::WheelShape::Ellipse,Wheel::WheelShape::Polygon})
        for(const bool aa:{false,true})for(const float scale:{.4f,1.f,2.f,4.f})
        for(const float progress:{.001f,.01f,.1f,.5f,1.f})for(int slot=0;slot<10;++slot) {
            const float step=2*std::numbers::pi_v<float>/10;
            const float begin=slot*step-step*.5f+.02f,end=begin+(step-.04f)*progress;
            const float inner=116*scale,outer=std::lerp(124*scale,264*scale,progress);
            const int segments=std::max(12,int(std::ceil((end-begin)*outer/8)));
            Wheel::LayoutSkin skin;skin.shape=shape;skin.stretchX=1.25f;skin.stretchY=.9f;
            std::vector<ImVec2> points;
            for(int i=0;i<=segments;++i) {
                const float angle=std::lerp(begin,end,float(i)/segments);
                const auto point=Wheel::ShapePoint(skin,outer,angle);points.push_back({point.x,point.y});
            }
            for(int i=segments;i>=0;--i) {
                const float angle=std::lerp(begin,end,float(i)/segments);
                const auto point=Wheel::ShapePoint(skin,inner,angle);points.push_back({point.x,point.y});
            }
            ImDrawList actual(shared),reference(shared);
            actual._ResetForNewFrame();reference._ResetForNewFrame();
            actual.Flags=reference.Flags=aa?ImDrawListFlags_AntiAliasedFill:ImDrawListFlags_None;
            Wheel::FillSectorStrip(&actual,points.data(),segments+1,color);
            reference.AddConcavePolyFilled(points.data(),int(points.size()),color);
            Require(actual.VtxBuffer.Size==reference.VtxBuffer.Size && actual.IdxBuffer.Size==reference.IdxBuffer.Size);
            for(int i=0;i<actual.VtxBuffer.Size;++i) {
                const auto& a=actual.VtxBuffer[i];const auto& b=reference.VtxBuffer[i];
                Require(std::isfinite(a.pos.x) && std::isfinite(a.pos.y));
                Require(a.pos.x==b.pos.x && a.pos.y==b.pos.y && a.col==b.col);
                Require(a.uv.x==b.uv.x && a.uv.y==b.uv.y);
            }
            for(const auto index:actual.IdxBuffer)Require(index<actual.VtxBuffer.Size);
            // Different diagonals must cover the same signed interior area.
            const int fillIndices=segments*6;
            double stripArea=0,referenceArea=0;
            for(int i=0;i<fillIndices;i+=3) {stripArea+=Area(actual,i);referenceArea+=Area(reference,i);}
            Require(std::abs(stripArea-referenceArea)<std::max(.02,std::abs(referenceArea)*.00001));
            ++cases;
        }
    // Verify PrimReserve's 16-bit vertex-offset rollover is respected.
    ImDrawList rollover(shared);rollover._ResetForNewFrame();
    rollover.Flags=ImDrawListFlags_AllowVtxOffset | ImDrawListFlags_AntiAliasedFill;
    rollover.PrimReserve(0,65530);
    for(int i=0;i<65530;++i)rollover.PrimWriteVtx({0,0},{0,0},color);
    const ImVec2 points[]={{0,-200},{100,-170},{50,-85},{0,-100}};
    Wheel::FillSectorStrip(&rollover,points,2,color);
    Require(rollover.CmdBuffer.back().VtxOffset==65530);
    for(const auto index:rollover.IdxBuffer)Require(index<8);
    const auto vertices=rollover.VtxBuffer.Size,indices=rollover.IdxBuffer.Size;
    Wheel::FillSectorStrip(&rollover,points,1,color);
    Wheel::FillSectorStrip(&rollover,points,2,0);
    Require(rollover.VtxBuffer.Size==vertices && rollover.IdxBuffer.Size==indices);
    ImGui::DestroyContext();
    std::cout<<"Sector mesh: "<<cases<<" AA/non-AA poses match boundary/area; index rollover and empty fills passed\n";
}

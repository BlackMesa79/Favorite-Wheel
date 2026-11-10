#pragma once
#ifdef WHEEL_SWF_DIAGNOSTICS
#include "SwfRaster.h"
namespace Wheel::Swf {
// SWF twip origin; explicit pixel dimensions, no content-based registration.
struct DiagnosticViewport {
    double xMin=0,yMin=0,pixelsPerTwip=1;
    std::uint32_t width=0,height=0;
};
enum class DiagnosticFault { None, Mirror, DropPaths, Scale09, Offset4px };
// Per-Render work counters, compared against the production budgets.
struct DiagnosticWork { std::size_t instances=0,ops=0,points=0,coverage=0,paths=0; };
Result RenderMeasured(const Movie&,std::string_view label,Variant,std::uint32_t edge,DiagnosticWork&);
Result RenderViewport(const Movie&,std::string_view label,Variant,const DiagnosticViewport&,DiagnosticFault=DiagnosticFault::None);
}
#endif

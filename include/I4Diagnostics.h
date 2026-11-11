#pragma once
#include "I4Rules.h"
#include <format>

namespace Wheel::I4 {
    inline std::string DebugText(std::string_view text) {
        std::string result;
        for(char c:text)switch(c) {
        case '\n':result+="\\n";break;
        case '\r':result+="\\r";break;
        case '\t':result+="\\t";break;
        case '"':result+="\\\"";break;
        case '\\':result+="\\\\";break;
        default:result+=c;
        }
        return result;
    }
    // Only called behind Runtime().debug; escaped strings keep each item on one log line.
    inline std::string FormatResolveDebug(std::uint32_t formId,std::string_view name,const Icon& icon,std::span<const RuleMatch> matches) {
        std::string rules;
        for(const auto& match:matches) {
            if(!rules.empty())rules+=", ";
            rules+=std::format("{}#{}({})",DebugText(match.file),match.index,match.info?(match.icon?"info+icon":"info"):"icon");
        }
        return std::format("I4Debug formId={:08X} name=\"{}\" source=\"{}\" label=\"{}\" color={} origin={} rules=[{}]",
            formId,DebugText(name),DebugText(icon.source),DebugText(icon.label),icon.color?std::format("#{:06X}",*icon.color):"none",
            icon.origin==IconOrigin::Rule?"Rule":icon.origin==IconOrigin::Default?"Default":"None",rules);
    }
}

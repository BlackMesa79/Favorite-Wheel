#pragma once
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Wheel::I4 {
    struct CountedLoadWarning {
        std::string text;
        std::size_t count = 0;
    };
    // Presentation only: retain first occurrence order without changing LoadReport.
    inline std::vector<CountedLoadWarning> CountLoadWarnings(std::span<const std::string> warnings) {
        std::vector<CountedLoadWarning> result;
        std::map<std::string, std::size_t, std::less<>> indices;
        for(const auto& warning:warnings) {
            const auto [it,inserted]=indices.try_emplace(warning,result.size());
            if(inserted)result.push_back({warning,0});
            ++result[it->second].count;
        }
        return result;
    }
    struct LoadLogSummary {
        std::vector<std::string> warnings;
        std::size_t missingReferences = 0;
        std::size_t missingPlugins = 0;
    };
    inline LoadLogSummary SummarizeLoadWarnings(std::span<const std::string> warnings) {
        constexpr std::string_view missingMarker=": Plugin file not found: ";
        constexpr std::string_view unresolvedPrefix="I4 unresolved/unloaded plugin (possibly merged; no remapping): ";
        struct Line {
            std::string text;
            std::size_t count;
            bool missing;
        };
        std::vector<Line> lines;
        std::map<std::string, std::size_t, std::less<>> plugins;
        LoadLogSummary result;
        for(const auto& warning:CountLoadWarnings(warnings)) {
            const std::string_view text=warning.text;
            std::string_view plugin;
            std::size_t references=0;
            if(text.starts_with(unresolvedPrefix))plugin=text.substr(unresolvedPrefix.size());
            else if(const auto at=text.find(missingMarker);at!=text.npos && at>0) {
                plugin=text.substr(at+missingMarker.size());
                references=warning.count;
            }
            if(plugin.empty()) {
                lines.push_back({warning.text,warning.count,false});
                continue;
            }
            // Same case/slash normalization as the game's plugin lookup.
            std::string key(plugin);
            for(auto& c:key) {if(c>='A' && c<='Z')c+='a'-'A';if(c=='\\')c='/';}
            const auto [it,inserted]=plugins.try_emplace(key,lines.size());
            if(inserted)lines.push_back({std::string(plugin),0,true});
            lines[it->second].count+=references;
            result.missingReferences+=references;
        }
        result.missingPlugins=plugins.size();
        for(const auto& line:lines) {
            if(line.missing)result.warnings.push_back("missing/unloaded plugin: "+line.text+
                "; references="+std::to_string(line.count)+" (possibly merged; no remapping)");
            else result.warnings.push_back(line.text+(line.count>1?" (x"+std::to_string(line.count)+")":""));
        }
        return result;
    }
}

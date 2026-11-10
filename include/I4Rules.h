#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Wheel::I4 {
    struct Value {
        using Keywords = std::set<std::string, std::less<>>;
        // Arrays occur in Scaleform entry values; JSON condition arrays have different semantics.
        using Array = std::vector<Value>;
        using Storage = std::variant<std::monostate, bool, double, std::string, Keywords, Array>;
        Storage data;
        Value() = default;
        Value(std::nullptr_t) {}
        Value(bool v) : data(v) {}
        template <typename T> requires (std::integral<T> || std::floating_point<T>)
        Value(T v) : data(static_cast<double>(v)) {}
        Value(const char* v) : data(std::string(v)) {}
        Value(std::string v) : data(std::move(v)) {}
        Value(std::string_view v) : data(std::string(v)) {}
        Value(Keywords v) : data(std::move(v)) {}
        Value(Array v) : data(std::move(v)) {}
        bool operator==(const Value&) const = default;
        const double* Number() const { return std::get_if<double>(&data); }
        const std::string* String() const { return std::get_if<std::string>(&data); }
        bool IsNull() const { return std::holds_alternative<std::monostate>(data); }
    };
    struct Entry {
        std::map<std::string, Value, std::less<>> fields;
        const Value* Get(std::string_view name) const;
        void Set(std::string_view name, Value value);
        void Erase(std::string_view name);
    };
    enum class IconOrigin : std::uint8_t { None, Default, Rule };
    struct Icon {
        std::string source, label;
        std::optional<std::uint32_t> color;
        IconOrigin origin = IconOrigin::None;
    };
    struct LoadReport {
        std::size_t files = 0, rules = 0, rejected = 0;
        std::vector<std::string> warnings;
    };
    struct LoadOptions {
        // Full prefix: e.g. 0x02000000 (ESP), 0xFE123000 (ESL/ESPFE).
        // Resolve names case-insensitively (Skyrim plugin names are case-insensitive).
        // Missing/unloaded plugin: nullopt. Upstream adds the local ID, without masking.
        std::function<std::optional<std::uint32_t>(std::string_view)> pluginPrefix;
        // Receives the unprefixed iconSource (relative to Interface/).
        // Omitted: assume present, with an explicit report warning; never read artwork here.
        std::function<bool(std::string_view)> sourceExists;
    };
    // Replaces the whole immutable ruleset, preserving supplied file/rule order.
    LoadReport Load(std::span<const std::pair<std::string, std::string>> pluginJson);
    LoadReport Load(std::span<const std::pair<std::string, std::string>> pluginJson, const LoadOptions& options);
    // Concurrent calls are safe for distinct entries, including concurrent Load publication.
    Icon Resolve(Entry& entry);
}

/*
MIT License

Copyright (c) 2023 Parapets

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

Ported from Exit-9B/InventoryInjector@52cc4cb30f2ec09eba104606c8cdd17050c3a2bf.
*/

#include "I4Rules.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace Wheel::I4 {
const Value* Entry::Get(std::string_view name) const {
    auto it = fields.find(name);
    return it == fields.end() ? nullptr : &it->second;
}
void Entry::Set(std::string_view name, Value value) { fields.insert_or_assign(std::string(name), std::move(value)); }
void Entry::Erase(std::string_view name) { if (auto it = fields.find(name); it != fields.end()) fields.erase(it); }

namespace {
using Json = nlohmann::json;
#include "I4RuleTables.inc"
constexpr unsigned precedence[] = {2,1,3,4,7,8,9,5,6,11,13,0,12,10,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};
std::uint32_t Bits(double n) {
    if (!std::isfinite(n)) return 0;
    n = std::fmod(std::trunc(n), 4294967296.0);
    return static_cast<std::uint32_t>(n < 0 ? n + 4294967296.0 : n);
}
std::string Lower(std::string_view s) {
    std::string result(s);
    for (char& c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return result;
}
std::optional<double> Lookup(std::string_view table, std::string_view name) {
    const auto key = Lower(name);
    for (const auto& e : enumValues) if (e.table == table && e.name == key) return e.value;
    return {};
}
std::string_view SubTable(int type) {
    switch (type) {
    case 26: return "EquipType"; case 27: return "BookType"; case 32: return "MiscType";
    case 41: return "WeaponType"; case 42: return "AmmoType"; case 46: return "PotionType";
    case 52: return "SoulGemType"; default: return {};
    }
}
std::string_view DisplayTable(int type) {
    switch (type) {
    case 26: return "ArmorSubTypeDisplayMap"; case 27: return "BookSubTypeDisplayMap";
    case 32: return "MiscSubTypeDisplayMap"; case 41: return "WeaponSubTypeDisplayMap";
    case 42: return "AmmoSubTypeDisplayMap"; case 46: return "PotionSubTypeDisplayMap";
    case 52: return "SoulGemSubTypeDisplayMap"; default: return {};
    }
}
std::string_view Table(std::string_view name, int type) {
    if (name == "formType") return "FormType";
    if (name == "weightClass") return "WeightClass";
    if (name == "weaponType") return "WEAPON_TYPE";
    if (name == "bookType") return "OBJ_BOOK_Type";
    if (name == "gemSize") return "SoulGemType";
    if (name == "status") return "SoulGemStatus";
    if (name == "school" || name == "resistance" || name == "actorValue" || name == "teachesSkill") return "ActorValue";
    if (name == "spellType") return "SpellType";
    if (name == "archetype") return "EffectArchetype";
    if (name == "deliveryType") return "Delivery";
    if (name == "castType") return "CastingType";
    if (name == "effectFlags") return "EffectFlag";
    if (name == "flags") {
        if (type == 27) return "BookFlag";
        if (type == 42) return "AmmoFlag";
        if (type == 46) return "PotionFlag";
    }
    return name == "subType" ? SubTable(type) : std::string_view{};
}
std::uint32_t Color(std::string_view s) {
    if (s.empty()) return 0;
    if (s.front() == '#') s.remove_prefix(1);
    return static_cast<std::uint32_t>(std::stoll(std::string(s), nullptr, 16));
}
struct Predicate {
    enum class Kind { Match, Any, Range, Bits, Keywords, MainPart } kind = Kind::Match;
    Value value;
    std::optional<double> min, max;
    std::vector<Predicate> children;
    bool Matches(const Value* v) const {
        if (kind == Kind::Any) return std::ranges::any_of(children, [v](const auto& p) { return p.Matches(v); });
        if (kind == Kind::Match) {
            if (!v) return value.IsNull();
            if (const auto* a = std::get_if<Value::Array>(&v->data))
                return std::ranges::any_of(*a, [this](const auto& item) { return Matches(&item); });
            return *v == value;
        }
        if (!v) return false;
        if (kind == Kind::Keywords) {
            const auto* keys = std::get_if<Value::Keywords>(&v->data);
            return keys && keys->contains(*value.String());
        }
        const auto* n = v->Number();
        if (!n) return false;
        if (kind == Kind::Range) return (!min || *n >= *min) && (!max || *n <= *max);
        auto bits = Bits(*n), mask = Bits(*value.Number());
        if (kind == Kind::Bits) return (bits & mask) != 0;
        if (!(bits & mask)) return false;
        for (auto bit : precedence) if (bits & (1u << bit)) return mask == (1u << bit);
        return false;
    }
};
struct Rule {
    std::map<std::string, Predicate, std::less<>> match;
    Entry assign;
    bool hasInfo = false;
    std::optional<std::string> requiredKeyword, requiredEffectKeyword;
    bool Matches(const Entry& entry) const {
        for (const auto& [name, p] : match) if (!p.Matches(entry.Get(name))) return false;
        return true;
    }
};
struct Ruleset {
    std::vector<Rule> rules;
    std::vector<std::size_t> common;
    std::map<double, std::vector<std::size_t>> byType, byId;
    void Index() {
        // Reclassification is legal. Disable only the dimension which assignments can change.
        bool mutatesType = false, mutatesId = false, mutatesKeywords = false, mutatesEffectKeywords = false;
        for (const auto& r : rules) {
            mutatesType |= r.assign.Get("formType") != nullptr;
            mutatesId |= r.assign.Get("formId") != nullptr;
            mutatesKeywords |= r.assign.Get("keywords") != nullptr;
            mutatesEffectKeywords |= r.assign.Get("effectKeywords") != nullptr;
        }
        for (std::size_t i = 0; i < rules.size(); ++i) {
            auto keyword = [&](std::string_view name, bool mutableField, auto& destination) {
                auto it = rules[i].match.find(name);
                if (!mutableField && it != rules[i].match.end() && it->second.kind == Predicate::Kind::Keywords)
                    destination = *it->second.value.String();
            };
            keyword("keywords", mutatesKeywords, rules[i].requiredKeyword);
            keyword("effectKeywords", mutatesEffectKeywords, rules[i].requiredEffectKeyword);
            auto numericKeys = [&](std::string_view name) -> std::optional<std::vector<double>> {
                auto it = rules[i].match.find(name);
                if (it == rules[i].match.end()) return {};
                std::vector<double> keys;
                auto collect = [&](auto&& self, const Predicate& p) -> bool {
                    if (p.kind == Predicate::Kind::Match && p.value.Number()) {
                        keys.push_back(*p.value.Number()); return true;
                    }
                    if (p.kind != Predicate::Kind::Any) return false;
                    for (const auto& child : p.children) if (!self(self, child)) return false;
                    return true;
                };
                if (!collect(collect, it->second)) return {};
                std::sort(keys.begin(), keys.end());
                keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
                return keys;
            };
            if (auto ids = numericKeys("formId"); ids && !mutatesId) {
                for (auto id : *ids) byId[id].push_back(i);
            } else if (auto types = numericKeys("formType"); types && !mutatesType) {
                for (auto type : *types) byType[type].push_back(i);
            } else common.push_back(i);
        }
    }
    std::vector<std::size_t> Candidates(const Entry& e) const {
        std::vector<std::size_t> result;
        result.reserve(common.size()+8);
        auto keys = [&](std::string_view name) -> const Value::Keywords* {
            auto value = e.Get(name);
            return value ? std::get_if<Value::Keywords>(&value->data) : nullptr;
        };
        const auto* keywords = keys("keywords");
        const auto* effectKeywords = keys("effectKeywords");
        auto add = [&](const auto& ids) {
            for (auto id : ids) {
                const auto& r = rules[id];
                if (r.requiredKeyword && (!keywords || !keywords->contains(*r.requiredKeyword))) continue;
                if (r.requiredEffectKeyword && (!effectKeywords || !effectKeywords->contains(*r.requiredEffectKeyword))) continue;
                result.push_back(id);
            }
        };
        add(common);
        auto append = [&](const auto& index, std::string_view key) {
            const auto* v = e.Get(key);
            if (v && std::holds_alternative<Value::Array>(v->data)) {
                // Rare Scaleform array values still obey MatchProperty's element-any semantics.
                for (const auto& [n, ids] : index) add(ids);
            } else if (v && v->Number()) {
                if (auto it = index.find(*v->Number()); it != index.end()) add(it->second);
            }
        };
        append(byId, "formId"); append(byType, "formType");
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }
};
std::atomic<std::shared_ptr<const Ruleset>> current{std::make_shared<const Ruleset>()};

// JsonCpp CharReaderBuilder defaults: comments, trailing commas, BOM, duplicate keys
// (last wins), and failIfExtra=false. Preserve offsets/newlines for parse diagnostics.
std::string CompatibleJson(std::string text) {
    bool quoted = false, escaped = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '/' && i + 1 < text.size() && (text[i+1] == '/' || text[i+1] == '*')) {
            const bool block = text[i+1] == '*';
            text[i++] = ' '; text[i] = ' ';
            bool ended = !block;
            while (++i < text.size()) {
                if (!block && (text[i] == '\n' || text[i] == '\r')) break;
                if (block && text[i] == '*' && i + 1 < text.size() && text[i+1] == '/') {
                    text[i++] = ' '; text[i] = ' '; ended = true; break;
                }
                if (text[i] != '\n' && text[i] != '\r') text[i] = ' ';
            }
            if (!ended) throw std::runtime_error("Unterminated JSON comment");
        }
    }
    quoted = escaped = false;
    int depth = 0;
    char previous = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else {
            if (c == '"') quoted = true;
            else if (c == '{' || c == '[') ++depth;
            else if (c == '}' || c == ']') {
                if (--depth == 0) { text.resize(i+1); break; }
            } else if (c == ',' && previous != ',' && previous != '[' && previous != '{') {
                const auto next = text.find_first_not_of(" \r\n\t", i+1);
                if (next != std::string::npos && (text[next] == ']' || text[next] == '}')) text[i] = ' ';
            }
        }
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') previous = c;
    }
    return text;
}
struct Parser {
    const LoadOptions& options;
    LoadReport& report;
    std::string file;
    std::map<std::string, bool> sources;
    std::uint32_t FormId(const std::string& text) {
        const auto sep = text.find('|');
        if (sep == std::string::npos) throw std::runtime_error("Expected plugin|hexID: " + text);
        std::uint32_t local = 0;
        std::istringstream stream(text.substr(sep+1));
        if (!(stream >> std::hex >> local)) throw std::runtime_error("Invalid local form ID: " + text);
        const auto plugin = text.substr(0,sep);
        auto prefix = options.pluginPrefix ? options.pluginPrefix(plugin) : std::nullopt;
        if (!prefix) { report.warnings.push_back(file + ": Plugin file not found: " + plugin); return 0; }
        // Deliberately ADD, not OR/mask: this is ParseUtil.cpp at the pinned revision.
        return *prefix + local;
    }
    using Sink = std::function<void(std::string_view, Predicate)>;
    void Property(std::string_view name, const Json& spec, int type, const Sink& add) {
        Predicate p;
        if (spec.is_array()) {
            for (const auto& item : spec) Property(name, item, type, add);
            return;
        }
        if (spec.is_object()) {
            if (auto it = spec.find("anyOf"); it != spec.end() && it->is_array()) {
                p.kind = Predicate::Kind::Any;
                for (const auto& item : *it) Property(name, item, type, [&](std::string_view, Predicate sub) { p.children.push_back(std::move(sub)); });
                add(name, std::move(p));
            }
            Predicate range; range.kind = Predicate::Kind::Range;
            if (auto it = spec.find("min"); it != spec.end() && it->is_number()) range.min = it->get<double>();
            if (auto it = spec.find("max"); it != spec.end() && it->is_number()) range.max = it->get<double>();
            if (range.min || range.max) add(name, std::move(range));
            return;
        }
        if (spec.is_string()) {
            const auto s = spec.get<std::string>();
            const auto table = Table(name, type);
            p.value = s;
            if (name == "formId" || name == "equipSlot" || name == "teachesSpell") p.value = FormId(s);
            else if (name == "iconColor") p.value = Color(s);
            else if (name == "keywords" || name == "effectKeywords") p.kind = Predicate::Kind::Keywords;
            else if (name == "formType") p.value = Lookup(table, s).value_or(-1);
            else if (!table.empty()) {
                const bool bitfield = table == "EffectFlag" || table == "BookFlag" || table == "AmmoFlag" || table == "PotionFlag";
                if (auto n = Lookup(table, s)) { p.value = *n; if (bitfield) p.kind = Predicate::Kind::Bits; }
                else if (!bitfield && Lower(s) == "other") p.value = Value{};
            }
        } else if (spec.is_boolean()) p.value = spec.get<bool>();
        else if (spec.is_number()) {
            const double n = spec.get<double>(); p.value = n;
            if (name == "parts" || name == "mainPart") {
                const auto part = Bits(n);
                if (part < 30 || part > 61) throw std::runtime_error("Part number out of range (30..61)");
                p.kind = name == "parts" ? Predicate::Kind::Bits : Predicate::Kind::MainPart;
                p.value = 1u << (part-30); name = "partMask";
            }
        } else if (!spec.is_null()) return;
        add(name, std::move(p));
    }
    void Assignment(Rule& rule, std::string_view name, const Json& spec, int type) {
        if (spec.is_boolean()) rule.assign.Set(name, spec.get<bool>());
        else if (spec.is_number()) rule.assign.Set(name, spec.get<double>());
        else if (spec.is_string()) {
            auto s = spec.get<std::string>();
            if (name == "iconColor") { rule.assign.Set(name, Color(s)); return; }
            // Upstream WeightClassParser intentionally uses EquipType (including its quirks).
            auto table = name == "subType" ? SubTable(type) : name == "weightClass" ? "EquipType" : "";
            if (auto n = Lookup(table, s)) {
                rule.assign.Set(name, *n);
                auto display = name == "weightClass" ? "ArmorSubTypeDisplayMap" : DisplayTable(type);
                for (const auto& d : displayValues) if (d.table == display && d.name == Lower(s)) rule.assign.Set(std::string(name)+"Display", d.value);
            } else rule.assign.Set(name, std::move(s));
        }
        // Null, arrays and objects are ignored by CustomDataParser, not assigned.
    }
    Rule Parse(const Json& input) {
        Rule rule;
        if (!input.is_object() || !input.contains("match") || !input["match"].is_object() || !input.contains("assign") || !input["assign"].is_object()) return rule;
        const auto& match = input["match"];
        int type = 0;
        // JsonCpp's preliminary match["formType"].asString() rejects containers.
        if (auto it = match.find("formType"); it != match.end() && (it->is_array() || it->is_object()))
            throw std::runtime_error("formType must be scalar (I4 RuleParser::asString)");
        if (auto it = match.find("formType"); it != match.end() && it->is_string()) type = static_cast<int>(Lookup("FormType", it->get<std::string>()).value_or(0));
        for (const auto& [name, spec] : match.items()) if (!name.empty() && name.front() != '$')
            Property(name, spec, type, [&](std::string_view key, Predicate p) { rule.match.insert_or_assign(std::string(key), std::move(p)); });
        for (const auto& [name, spec] : input["assign"].items()) if (!name.empty() && name.front() != '$') Assignment(rule, name, spec, type);
        if (auto* source = rule.assign.Get("iconSource")) {
            bool valid = false;
            if (const auto* s = source->String()) {
                auto [it, added] = sources.try_emplace(*s, true);
                if (added && options.sourceExists) it->second = options.sourceExists(*s);
                valid = it->second;
            }
            if (!valid) {
                report.warnings.push_back(file + ": invalid iconSource; removed iconSource/iconLabel");
                rule.assign.Erase("iconSource"); rule.assign.Erase("iconLabel");
            }
        } else if (rule.assign.Get("iconLabel")) rule.assign.Set("iconSource", Value{});
        for (const auto& [name, v] : rule.assign.fields) rule.hasInfo |= !name.starts_with("icon");
        return rule;
    }
};

int Number(const Entry& e, std::string_view name) {
    const auto* v = e.Get(name);
    return v && v->Number() ? static_cast<std::int32_t>(Bits(*v->Number())) : -1;
}
void DefaultIcon(Entry& e) {
    auto label = [&](std::string_view s) { e.Set("iconLabel", s); };
    auto color = [&](std::uint32_t c) { e.Set("iconColor", c); };
    const int type = Number(e,"formType"), sub = Number(e,"subType");
    auto resist = [&](bool scroll) {
        switch (Number(e,"resistance")) {
        case 41: label("magic_fire"); color(0xC73636); break;
        case 42: label("magic_shock"); color(scroll ? 0xFFFF00 : 0xEAAB00); break;
        case 43: label("magic_frost"); color(0x1FFBFF); break;
        }
    };
    switch (type) {
    case 22: {
        label("default_power");
        constexpr std::string_view schools[] = {"default_alteration","default_conjuration","default_destruction","default_illusion","default_restoration"};
        auto school = Number(e,"school");
        if (school >= 18 && school <= 22) label(schools[school-18]);
        if (school == 20) resist(false);
        break;
    }
    case 23: label("default_scroll"); resist(true); break;
    case 26: {
        label("default_armor"); color(0xEDDA87);
        int weight = Number(e,"weightClass");
        const auto* weightValue = e.Get("weightClass");
        if (!weightValue || !weightValue->Number()) break;
        if (weight == 0) color(0x756000);
        if (weight == 1) color(0x6B7585);
        if (weight == 4 || ((weight == 0 || weight == 1) && (sub == 9 || sub == 10 || sub == 11 || sub == 12))) {
            if (sub == 9) label("armor_circlet");
            if (sub == 10) label("armor_amulet");
            if (sub == 12) label("armor_ring");
        } else {
            constexpr std::string_view parts[] = {"head","head","head","body","forearms","hands","shield","calves","feet","","","","","body"};
            if (sub >= 0 && sub < 14 && !parts[sub].empty()) label(std::string(weight == 0 ? "lightarmor_" : weight == 1 ? "armor_" : "clothing_")+std::string(parts[sub]));
        }
        break;
    }
    case 27: label(sub == 0 ? "book_tome" : sub == 1 || sub == 2 ? "book_note" : "default_book"); break;
    case 30: label("default_ingredient"); break;
    case 31: label("misc_torch"); break;
    case 41: {
        constexpr std::string_view labels[] = {"default_weapon","weapon_sword","weapon_dagger","weapon_waraxe","weapon_mace","weapon_greatsword","weapon_battleaxe","weapon_hammer","weapon_bow","weapon_crossbow","weapon_staff","weapon_pickaxe","weapon_woodaxe"};
        label(sub >= 0 && sub < 13 ? labels[sub] : "default_weapon"); color(0xA4A5BF); break;
    }
    case 42: label(sub == 1 ? "weapon_bolt" : "weapon_arrow"); color(0xA89E8C); break;
    case 45: label("default_key"); break;
    case 46: {
        constexpr std::string_view labels[] = {"potion_health","potion_health","potion_health","potion_magic","potion_magic","potion_magic","potion_stam","potion_stam","potion_stam","potion_fire","potion_shock","potion_frost","default_potion","food_wine","default_food","potion_poison"};
        constexpr std::uint32_t colors[] = {0xDB2E73,0xDB2E73,0xDB2E73,0x2E9FDB,0x2E9FDB,0x2E9FDB,0x51DB2E,0x51DB2E,0x51DB2E,0xC73636,0xEAAB00,0x1FFBFF};
        label(sub >= 0 && sub < 16 ? labels[sub] : "default_potion");
        if (sub >= 0 && sub < 12) color(colors[sub]);
        if (sub == 15) color(0xAD00B3);
        break;
    }
    case 52: {
        label("default_soulgem"); color(0xE3E0FF);
        constexpr std::uint32_t colors[] = {0xD7D4FF,0xC0BAFF,0xABA3FF,0x948BFC,0x7569FF,0x7569FF};
        if (sub >= 1 && sub <= 6) color(colors[sub-1]);
        if (sub == 6) label("soulgem_azura");
        else if (sub >= 1 && sub <= 5) {
            constexpr std::string_view status[] = {"empty","partial","full"};
            auto s = Number(e,"status");
            if (s >= 0 && s < 3) label(std::string(sub >= 4 ? "soulgem_grand" : "soulgem_")+std::string(status[s]));
        }
        break;
    }
    case 32: {
        label("default_misc");
        struct Item { int sub; std::string_view label; std::uint32_t color; };
        constexpr Item items[] = {{0,"misc_gem",0xFFB0D1},{1,"misc_dragonclaw",0},{2,"misc_artifact",0},{3,"misc_leather",0xBA8D23},{4,"misc_strips",0xBA8D23},{5,"misc_hide",0xDBB36E},{6,"misc_remains",0},{7,"misc_ingot",0x828282},{9,"clothing_body",0xEDDA87},{11,"misc_wood",0xA89E8C},{19,"misc_clutter",0},{20,"misc_lockpick",0},{21,"misc_gold",0xCCCC33}};
        for (const auto& i : items) if (sub == i.sub) { label(i.label); if (i.color) color(i.color); break; }
        break;
    }
    }
}
} // namespace

LoadReport Load(std::span<const std::pair<std::string, std::string>> pluginJson) { return Load(pluginJson, LoadOptions{}); }
LoadReport Load(std::span<const std::pair<std::string, std::string>> pluginJson, const LoadOptions& options) {
    LoadReport report;
    auto rules = std::make_shared<Ruleset>();
    if (!options.sourceExists && !pluginJson.empty()) report.warnings.emplace_back("sourceExists not supplied: icon sources assumed present (offline)");
    if (!options.pluginPrefix && !pluginJson.empty()) report.warnings.emplace_back("pluginPrefix not supplied: plugin-qualified IDs resolve to zero (offline)");
    Parser parser{options, report, {}, {}};
    for (const auto& [name, text] : pluginJson) {
        parser.file = name;
        try {
            auto root = Json::parse(CompatibleJson(text), nullptr, true, true);
            if (!root.is_object()) { report.warnings.push_back(name+": root is not an object"); continue; }
            ++report.files;
            auto array = root.find("rules");
            if (array == root.end() || !array->is_array()) continue;
            for (std::size_t i = 0; i < array->size(); ++i) {
                try {
                    auto rule = parser.Parse((*array)[i]);
                    if (rule.match.empty() || rule.assign.fields.empty()) { ++report.rejected; report.warnings.push_back(name+": rule "+std::to_string(i)+" has empty match/assign"); continue; }
                    rules->rules.push_back(std::move(rule)); ++report.rules;
                } catch (const std::exception& ex) { ++report.rejected; report.warnings.push_back(name+": rule "+std::to_string(i)+": "+ex.what()); }
            }
        } catch (const std::exception& ex) { report.warnings.push_back(name+": "+ex.what()); }
    }
    rules->Index(); current.store(std::move(rules));
    return report;
}
Icon Resolve(Entry& entry) {
    const auto rules = current.load();
    // SkyUI supplies initial icons before ProcessEntry. Seed absent icons for offline entries.
    if (!entry.Get("iconLabel")) {
        const auto* existingColor = entry.Get("iconColor");
        std::optional<Value> savedColor = existingColor ? std::optional<Value>(*existingColor) : std::nullopt;
        if (Number(entry,"formType") == 119 || Number(entry,"type") == 10) entry.Set("iconLabel", "default_shout");
        else if (Number(entry,"type") == 11) entry.Set("iconLabel", "default_effect");
        else if (Number(entry,"type") == 13) entry.Set("iconLabel", "default_power");
        else DefaultIcon(entry);
        if (savedColor) entry.Set("iconColor", std::move(*savedColor));
    }
    const auto candidates = rules->Candidates(entry);
    bool update = false, explicitIcon = false;
    for (auto index : candidates) {
        const auto& rule = rules->rules[index];
        if (!rule.hasInfo || !rule.Matches(entry)) continue;
        update = true;
        for (const auto& [name, value] : rule.assign.fields) if (!name.starts_with("icon")) entry.Set(name, value);
    }
    if (update) { entry.Erase("iconLabel"); entry.Erase("iconColor"); DefaultIcon(entry); }
    for (auto index : candidates) {
        const auto& rule = rules->rules[index];
        if (!rule.Matches(entry)) continue;
        for (const auto& [name, value] : rule.assign.fields) if (name.starts_with("icon")) {
            entry.Set(name, value);
            explicitIcon |= name == "iconSource" || name == "iconLabel" || name == "iconColor";
        }
    }
    Icon result;
    if (const auto* label = entry.Get("iconLabel"); label && label->String()) result.label = *label->String();
    if (result.label.empty()) return result;
    result.source = "skyui/icons_item_psychosteve.swf";
    if (const auto* source = entry.Get("iconSource"); source && source->String()) result.source = *source->String();
    if (const auto* color = entry.Get("iconColor"); color && color->Number()) result.color = Bits(*color->Number());
    result.origin = explicitIcon ? IconOrigin::Rule : IconOrigin::Default;
    return result;
}
} // namespace Wheel::I4

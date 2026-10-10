#include "NameHitMap.h"
#include "NavigationPresentation.h"
#include "Settings.h"
#include "Transition.h"
#include "UILayout.h"
#include "UIResources.h"
#include "Wheel.h"
#include "QuickSlots.h"
#include "WheelFonts.h"
#include "WheelIcons.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>
#include <numbers>
#include <string>

namespace Wheel
{
    namespace
    {
        constexpr const char *categoryKeys[] = {"weapons", "armor", "potions", "food", "spells", "shouts", "powers", "other"};
        static_assert(std::size(categoryKeys)==categoryCount);
        struct Feedback
        {
            std::array<float, slots> hover{};
            int category = -1, page = -1;
            float age = 1.f;
        } feedback;
        ImVec2 At(ImVec2 c, float r, float a)
        {
            return {c.x + std::sin(a) * r, c.y - std::cos(a) * r};
        }
        ImU32 Alpha(ImU32 color, float alpha)
        {
            return (color & 0x00FFFFFFu) | (static_cast<ImU32>(std::clamp(alpha, 0.f, 1.f) * 255) << 24);
        }
        // Move whole text/icon groups without scaling their baked pixel geometry.
        void Reveal(ImDrawList *d, int first, float alpha, ImVec2 offset = {0, 0})
        {
            for (int i = first; i < d->VtxBuffer.Size; ++i)
            {
                auto &vertex = d->VtxBuffer[i];
                vertex.pos.x += offset.x;
                vertex.pos.y += offset.y;
                vertex.col = (vertex.col & 0x00FFFFFFu) |
                             (static_cast<ImU32>((vertex.col >> 24) * std::clamp(alpha, 0.f, 1.f)) << 24);
            }
        }
        ImU32 Mix(ImU32 a, ImU32 b, float t)
        {
            ImU32 color = 0;
            for (int shift = 0; shift < 32; shift += 8)
                color |= static_cast<ImU32>(
                             std::lerp(float((a >> shift) & 255), float((b >> shift) & 255), std::clamp(t, 0.f, 1.f)))
                         << shift;
            return color;
        }
        void Text(ImDrawList *d, ImVec2 c, const std::string &text, float size, ImU32 color, float wrap = 0,
                  float shadow = .32f)
        {
            size = std::round(size);
            auto font = FontAt(size);
            const auto dim = font->CalcTextSizeA(size, FLT_MAX, wrap, text.c_str());
            const ImVec2 origin{std::round(c.x - dim.x / 2), std::round(c.y - dim.y / 2)};
            if (shadow > 0)
                d->AddText(font, size, {origin.x, origin.y + 1}, Alpha(IM_COL32_BLACK, shadow), text.c_str(), nullptr,
                           wrap);
            d->AddText(font, size, origin, color, text.c_str(), nullptr, wrap);
        }
        void TrimLast(std::string &text)
        {
            if (text.empty())
                return;
            auto pos = text.size() - 1;
            while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80)
                --pos;
            text.resize(pos);
        }
        std::string Fit(const std::string &source, float size, float width)
        {
            size = std::round(size);
            auto font = FontAt(size);
            if (font->CalcTextSizeA(size, FLT_MAX, 0, source.c_str()).x <= width)
                return source;
            auto text = source;
            do
            {
                TrimLast(text);
            } while (!text.empty() && font->CalcTextSizeA(size, FLT_MAX, 0, (text + "...").c_str()).x > width);
            return text + "...";
        }
        std::string Wrapped(const std::string &source, float size, float width, int lines)
        {
            size = std::round(size);
            auto font = FontAt(size);
            auto text = Fit(source, size, width * lines);
            if (font->CalcTextSizeA(size, FLT_MAX, width, text.c_str()).y <= size * lines)
                return text;
            do
            {
                TrimLast(text);
            } while (!text.empty() &&
                     font->CalcTextSizeA(size, FLT_MAX, width, (text + "...").c_str()).y > size * lines);
            return text + "...";
        }
        void LeftText(ImDrawList *d, ImVec2 at, const std::string &text, float size, ImU32 color, float width = 0)
        {
            size = std::round(size);
            d->AddText(FontAt(size), size, {std::round(at.x), std::round(at.y)}, color, text.c_str(), nullptr, width);
        }
        void Arc(ImDrawList *d, ImVec2 c, float radius, float begin, float end, ImU32 color, float thickness)
        {
            const int segments = std::max(4, int(std::ceil((end - begin) * radius / 9)));
            for (int i = 0; i <= segments; ++i)
                d->PathLineTo(At(c, radius, std::lerp(begin, end, float(i) / segments)));
            d->PathStroke(color, 0, thickness);
        }
        void Sector(ImDrawList *d, ImVec2 c, float inner, float outer, float begin, float end, ImU32 color,
                    float relief)
        {
            // Single AA outline; vertex tint supplies relief without textures or scene sampling.
            const int start = d->VtxBuffer.Size, segments = std::max(12, int(std::ceil((end - begin) * outer / 8)));
            for (int i = 0; i <= segments; ++i)
                d->PathLineTo(At(c, outer, std::lerp(begin, end, float(i) / segments)));
            for (int i = segments; i >= 0; --i)
                d->PathLineTo(At(c, inner, std::lerp(begin, end, float(i) / segments)));
            d->PathFillConcave(color | 0xFF000000u);
            const auto shade = Mix(color, IM_COL32(4, 8, 12, 255), relief * .16f),
                       light = Mix(color, IM_COL32(213, 225, 232, 255), relief * .075f);
            for (int i = start; i < d->VtxBuffer.Size; ++i)
            {
                auto &v = d->VtxBuffer[i];
                const float t = std::clamp((v.pos.y - c.y + outer) / (outer * 2), 0.f, 1.f);
                v.col = (Mix(light, shade, t) & 0x00FFFFFFu) | (v.col & 0xFF000000u);
            }
        }
        void Diamond(ImDrawList *d, ImVec2 c, float r, ImU32 color)
        {
            const ImVec2 p[] = {{c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}};
            d->AddPolyline(p, 4, color, ImDrawFlags_Closed, 1.f);
        }
        void Keycap(ImDrawList *d, ImVec2 at, const char *key, const Theme &t, float s, bool enabled = true)
        {
            const float r = 11 * s, w=std::max(r,static_cast<float>(std::strlen(key))*4.5f*s+4*s);
            d->AddRectFilled({at.x - w, at.y - r}, {at.x + w, at.y + r}, t.panel | 0xFF000000u, 3 * s);
            d->AddRect({at.x - w, at.y - r}, {at.x + w, at.y + r}, Alpha(t.border, enabled ? .85f : .35f), 3 * s, 0, s);
            Text(d, at, key, 14 * s, enabled ? t.accent : t.muted, 0, t.textShadow);
        }
        void CategoryStrip(ImDrawList *d, ImVec2 c, float s, const View &v, float pageFade)
        {
            const auto &t = Style(v.config);
            const int current =
                v.functions ? (v.functionSection == FaceLight::Section::Outfits ? 0 : 1) : v.visibleCategories.Index(v.category);
            const auto ribbon = RibbonLabels(current, v.functions ? FaceLight::TypeCount(v.faceLightAvailable) : v.visibleCategories.Count());
            const float y = c.y - 307 * s;
            std::array<std::string, 5> captions;
            std::array<float, 5> halfWidths{}, positions{}, sizes{};
            std::array<bool, 5> present{};
            for (int i = 0; i < ribbon.size; ++i)
            {
                const auto label = ribbon.labels[i];
                const int at = label.offset + 2;
                const bool selected = label.offset == 0;
                const char *key =
                    v.functions ? (label.index == 0 ? "outfitTitle" : "lightTitle") : categoryKeys[static_cast<int>(v.visibleCategories.At(label.index))];
                sizes[at] = selected ? 25 * s * t.titleScale : 16 * s;
                const float width = (selected ? 154.f : std::abs(label.offset) == 1 ? 82.f : 64.f) * s;
                captions[at] = Fit(Tr(v.config, key), sizes[at], width);
                if (selected)
                    captions[at] = "< " + captions[at] + " >";
                const float pixels = std::round(sizes[at]);
                halfWidths[at] = FontAt(pixels)->CalcTextSizeA(pixels, FLT_MAX, 0, captions[at].c_str()).x / (2 * s);
                present[at] = true;
            }
            float edge = halfWidths[2];
            for (int direction : {-1, 1})
            {
                float previousEdge = halfWidths[2];
                for (int distance = 1; distance <= 2; ++distance)
                {
                    const int at = 2 + direction * distance;
                    if (!present[at])
                        continue;
                    const float offset = previousEdge + 9.2f + halfWidths[at];
                    positions[at] = direction * offset;
                    previousEdge = offset + halfWidths[at];
                }
                edge = std::max(edge, previousEdge);
            }
            // Pack actual text edges, keeping the bracketed selection centred.
            const float keyOffset = edge + 22.f;
            if (v.config.showHints && ribbon.size > 1)
            {
                Keycap(d, {c.x - keyOffset * s, y}, v.gamepad?(v.config.gamepadCategoryButtons==1?"LT":"LB"):"A", t, s);
                Keycap(d, {c.x + keyOffset * s, y}, v.gamepad?(v.config.gamepadCategoryButtons==1?"RT":"RB"):"D", t, s);
            }
            for (int i = 0; i < ribbon.size; ++i)
            {
                const auto label = ribbon.labels[i];
                const bool selected = label.offset == 0;
                const int at = label.offset + 2;
                Text(d, {c.x + positions[at] * s, y}, captions[at], sizes[at],
                     Alpha(selected ? t.accent : t.muted, selected                      ? pageFade
                                                          : std::abs(label.offset) == 1 ? .82f
                                                                                        : .55f),
                     0, t.textShadow * (selected ? pageFade : .65f));
            }
        }
        void PageRail(ImDrawList *d, ImVec2 c, float s, const View &v)
        {
            const auto &t = Style(v.config);
            const int total = PageCount(ItemCount(v)), current = std::clamp(v.page, 0, total - 1);
            const auto pages = VisiblePages(total, current);
            const float x = c.x + 302 * s, top = c.y - (pages.size - 1) * 17 * s,
                        bottom = top + (pages.size - 1) * 34 * s;
            if (v.config.showHints)
            {
                Keycap(d, {x, top - 34 * s}, v.gamepad?"UP":"W", t, s, total > 1);
                Keycap(d, {x, bottom + 34 * s}, v.gamepad?"DN":"S", t, s, total > 1);
            }
            for (int i = 0; i < pages.size; ++i)
            {
                const ImVec2 at{x, top + i * 34 * s};
                const bool selected = pages.first + i == current;
                if (selected)
                {
                    const float r = 8 * s;
                    const ImVec2 p[] = {{at.x, at.y - r}, {at.x + r, at.y}, {at.x, at.y + r}, {at.x - r, at.y}};
                    d->AddConvexPolyFilled(p, 4, t.text);
                }
                Diamond(d, at, selected ? 9 * s : 7 * s, selected ? t.accent : Alpha(t.muted, .7f));
            }
            if (pages.first > 0)
                Text(d, {x, top - 19 * s}, "...", 12 * s, t.muted, 0, 0);
            if (pages.first + pages.size < total)
                Text(d, {x, bottom + 19 * s}, "...", 12 * s, t.muted, 0, 0);
            Text(d, {x, bottom + (v.config.showHints ? 58.f : 35.f) * s},
                 std::to_string(current + 1) + " / " + std::to_string(total), 12 * s, t.muted, 0, t.textShadow);
        }
        void Surface(ImDrawList *d, ImVec2 a, ImVec2 b, const Theme &t, float s, bool accent = false)
        {
            const float radius = t.cornerRadius * s;
            for (int i = 3; i > 0; --i)
            {
                const float pad = i * 2 * s;
                d->AddRect({a.x - pad, a.y - pad + 2 * s}, {b.x + pad, b.y + pad + 2 * s}, IM_COL32(0, 0, 0, 22),
                           radius + pad, 0, 2 * s);
            }
            d->AddRectFilled(a, b, t.panel | 0xFF000000u, radius);
            d->AddRect(a, b, accent ? Alpha(t.accent, .65f) : t.border, radius, 0, t.borderWidth * s);
            d->AddLine({a.x + radius + 8 * s, a.y + 2 * s}, {b.x - radius - 8 * s, a.y + 2 * s},
                       Alpha(t.accent, .15f * t.ornament), s);
        }
        void Rule(ImDrawList *d, ImVec2 c, float half, const Theme &t, float s)
        {
            d->AddLine({c.x - half, c.y}, {c.x - 7 * s, c.y}, Alpha(t.border, .55f), t.borderWidth * s);
            d->AddLine({c.x + 7 * s, c.y}, {c.x + half, c.y}, Alpha(t.border, .55f), t.borderWidth * s);
            if (t.ornament > 0)
                Diamond(d, c, 3 * s, Alpha(t.accent, t.ornament));
        }
        void PanelFrame(ImDrawList *d, ImVec2 c, float s, const Theme &t, const std::string &title,
                        const std::string &hint)
        {
            Surface(d, {c.x - 340 * s, c.y - 325 * s}, {c.x + 340 * s, c.y + 325 * s}, t, s);
            Text(d, {c.x, c.y - 277 * s}, Fit(title, 25 * s * t.titleScale, 610 * s), 25 * s * t.titleScale, t.text, 0,
                 t.textShadow);
            Text(d, {c.x, c.y - 241 * s}, Fit(hint, 14 * s, 620 * s), 14 * s, t.muted, 0, t.textShadow);
            Rule(d, {c.x, c.y - 222 * s}, 300 * s, t, s);
            Rule(d, {c.x, c.y + 214 * s}, 300 * s, t, s);
        }
        void Button(ImDrawList *d, ImVec2 c, float s, const View &v, Rect rect, const std::string &label,
                    bool accent = false, bool danger = false)
        {
            const auto &t = Style(v.config);
            const bool hover = rect.Contains(v.x * 224, v.y * 224);
            const ImVec2 a{c.x + rect.x * s, c.y + rect.y * s}, b{a.x + rect.w * s, a.y + rect.h * s};
            const float radius = std::min(t.cornerRadius * .5f, rect.h * .2f) * s;
            const ImU32 edge = danger ? IM_COL32(203, 129, 121, 255) : t.accent;
            const auto fill = accent ? Mix(t.sector, t.accent, .12f) : t.sector;
            d->AddRectFilled(a, b, (hover ? Mix(fill, t.hover, .65f) : fill) | 0xFF000000u, radius);
            d->AddRect(a, b, hover || accent ? Alpha(edge, .9f) : Alpha(t.border, .6f), radius, 0, t.borderWidth * s);
            if (hover)
                d->AddLine({a.x + 6 * s, b.y - 2 * s}, {b.x - 6 * s, b.y - 2 * s}, Alpha(edge, .65f), s);
            const float size = 16 * s * t.labelScale;
            Text(d, {(a.x + b.x) / 2, (a.y + b.y) / 2}, Fit(label, size, (rect.w - 12) * s), size,
                 danger   ? edge
                 : accent ? t.accent
                          : t.text,
                 0, t.textShadow);
        }
        void NameField(ImDrawList *d, ImVec2 c, float s, const View &v)
        {
            const auto &t = Style(v.config);
            const auto &editor = v.outfitName;
            const auto rect = outfitNameButton;
            const ImVec2 a{c.x + rect.x * s, c.y + rect.y * s}, b{a.x + rect.w * s, a.y + rect.h * s};
            d->AddRectFilled(a, b, t.sector | 0xFF000000u, 5 * s);
            d->AddRect(a, b, Alpha(t.accent, .8f), 5 * s, 0, t.borderWidth * s);
            d->AddLine({a.x + 12 * s, b.y - 3 * s}, {b.x - 12 * s, b.y - 3 * s}, Alpha(t.accent, .25f), s);
            const float size = std::round(18 * s);
            auto font = FontAt(size);
            auto width = [&](std::size_t offset) {
                return font->CalcTextSizeA(size, FLT_MAX, 0, editor.text.c_str(), editor.text.c_str() + offset).x;
            };
            const float room = (rect.w - 28) * s, caret = width(editor.cursor),
                        scroll = std::max(0.f, caret - room + 2 * s);
            const ImVec2 at{a.x + 14 * s - scroll, std::round((a.y + b.y - size) / 2)};
            d->PushClipRect({a.x + 10 * s, a.y + 4 * s}, {b.x - 10 * s, b.y - 4 * s}, true);
            if (editor.Selected())
                d->AddRectFilled({at.x + width(editor.Begin()), at.y - 2 * s},
                                 {at.x + width(editor.End()), at.y + size + 2 * s}, Mix(t.sector, t.accent, .35f));
            if (editor.text.empty())
                d->AddText(font, size, at, t.muted, Tr(v.config, "outfitNameEmpty").c_str());
            else
                d->AddText(font, size, at, t.text, editor.text.c_str());
            if (std::fmod(ImGui::GetTime(), 1.0) < .65)
                d->AddLine({at.x + caret, at.y - s}, {at.x + caret, at.y + size + s}, t.accent,
                           std::max(1.f, 1.3f * s));
            d->PopClipRect();
            std::vector<NameHit> hits;
            for (std::size_t i = 0;; i = editor.Next(i))
            {
                hits.push_back({rect.x + 14 + (width(i) - scroll) / s, i});
                if (i == editor.text.size())
                    break;
            }
            PublishNameHits(editor.text, std::move(hits));
        }
        void OutfitPanel(ImDrawList *d, ImVec2 c, float s, const View &v)
        {
            const auto &t = Style(v.config);
            auto tr = [&](const char *key) { return Tr(v.config, key); };
            const bool naming = v.outfitDialog == 1 || v.outfitDialog == 2;
            PanelFrame(d, c, s, t, tr(v.outfitDialog == 1 ? "outfitSave" : "outfitManage"),
                       tr(naming ? "outfitNameEditHelp" : "outfitManageHelp"));
            if (naming)
                NameField(d, c, s, v);
            else
                Button(d, c, s, v, outfitNameButton,
                       v.outfitName.text.empty() ? tr("outfitNameEmpty") : v.outfitName.text, true);
            if (v.outfitDialog == 3)
            {
                Button(d, c, s, v, outfitOverwriteButton, tr("outfitOverwrite"));
                Button(d, c, s, v, outfitDeleteButton, tr("outfitDelete"), false, true);
                Button(d, c, s, v, outfitExportButton, tr("outfitExport"));
            }
            else if (v.outfitDialog == 4 || v.outfitDialog == 5)
                Text(d, {c.x, c.y - 40 * s}, tr(v.outfitDialog == 4 ? "outfitConfirmOverwrite" : "outfitConfirmDelete"),
                     20 * s, t.accent, 550 * s, t.textShadow);
            else
            {
                Item item;
                item.action = ActionKind::Outfit;
                DrawIcon(d, {c.x, c.y - 38 * s}, item, 1.65f * s * t.iconScale, Alpha(t.accent, .65f));
            }
            Rule(d, {c.x, c.y + 74 * s}, 240 * s, t, s);
            Text(d, {c.x, c.y + 120 * s}, Wrapped(tr("outfitStorageHint"), 16 * s, 535 * s, 3), 16 * s, t.muted,
                 535 * s, t.textShadow);
            if (v.outfitDialog != 3)
                Button(d, c, s, v, cancelButton, tr("outfitBack"));
            Button(d, c, s, v, applyButton, tr(v.outfitDialog == 3 ? "outfitBack" : "outfitConfirm"), true);
        }
        void SettingsPanel(ImDrawList *d, ImVec2 c, float s, const View &v)
        {
            const auto &config = v.config;
            const auto &t = Style(config);
            auto tr = [&](const char *key) { return Tr(config, key); };
            PanelFrame(d, c, s, t, tr("settingsTitle"), tr(v.settingsTab==3?
                (v.appliedGamepadMove?"padSettingsHelpRight":"controllerControlsHelp"):v.gamepad?
                (v.appliedGamepadMove?"padSettingsHelpRight":"padSettingsHelp"):
                v.settingsTab==2?"gameplaySettingsHelp":v.settingsTab==1?"keyboardControlsHelp":"layoutSettingsHelp"));
            Button(d,c,s,v,generalTab,tr("settingsGeneral"),v.settingsTab==0);
            Button(d,c,s,v,controlsTab,tr("settingsKeyboard"),v.settingsTab==1);
            Button(d,c,s,v,gamepadTab,tr("settingsController"),v.settingsTab==3);
            Button(d,c,s,v,gameplayTab,tr("settingsGameplay"),v.settingsTab==2);
            constexpr const char *keys[] = {"wheelSize",      "sensitivity", "hints",      "language",
                                            "theme",          "hotkey",      "positionX",  "positionY",
                                            "overlayOpacity", "sounds",      "animations", "switchWheelKey",
                                            "favoriteModifier","actionHotkey","actionModifier","gamepadHotkey","gamepadModifier","gamepadActionModifier","inventoryScope","timeMode","slowTimePercent","gamepadCategoryButtons","keepOpen","gamepadMoveWhileOpen","hideEmptyCategories"};
            const auto language = LanguageLabel(config);
            auto key=[&](int scan){auto copy=config;copy.hotkey=scan;return KeyLabel(copy);};
            const std::string values[] = {std::to_string(int(std::round(config.wheelScale * 100))) + "%",
                                          std::to_string(int(std::round(config.sensitivity * 100))) + "%",
                                          tr(config.showHints ? "on" : "off"),
                                          language,
                                          t.name,
                                          KeyLabel(config),
                                          std::to_string(config.positionX) + "%",
                                          std::to_string(config.positionY) + "%",
                                          std::to_string(config.overlayOpacity) + "%",
                                          tr(config.sounds ? "on" : "off"),
                                          tr(config.animations ? "on" : "off"),
                                          key(config.switchKey),ModifierLabel(config,config.hotkeyModifier),
                                          config.actionHotkey<0?tr("followFavorite"):key(config.actionHotkey),ModifierLabel(config,config.actionModifier),
                                          PadLabel(config,config.gamepadHotkey,true),PadLabel(config,config.gamepadModifier),PadLabel(config,config.gamepadActionModifier),
                                          tr(config.allInventory?"scopeAll":"scopeFavorites"),
                                          tr(config.timeMode==0?"timePause":config.timeMode==1?"timeSlow":"timeNormal"),std::to_string(config.slowPercent)+"%",
                                          config.gamepadCategoryButtons==1?"LT / RT":"LB / RB",tr(config.keepOpen?"on":"off"),tr(config.gamepadMoveWhileOpen?"on":"off"),tr(config.hideEmptyCategories?"on":"off")};
            for (int slot = 0; slot < SettingCount(v.settingsTab); ++slot)
            {
                const int row=SettingRow(v.settingsTab,slot);
                const float top = -173.f + slot * 31;
                const float size = 16 * s * t.labelScale;
                if (slot % 2 == 0)
                    d->AddRectFilled({c.x - 300 * s, c.y + top * s}, {c.x + 300 * s, c.y + (top + 28) * s},
                                     Alpha(t.border, .07f), 3 * s);
                LeftText(d, {c.x - 281 * s, c.y + (top + 14) * s - std::round(size) / 2},
                         Fit(tr(keys[row]), size, 300 * s), size, t.text);
                if (!BindingRow(row))
                {
                    Button(d, c, s, v, MinusButton(slot), "-");
                    Button(d, c, s, v, PlusButton(slot), "+");
                }
                Button(d, c, s, v, ValueButton(slot),v.capturingKey && v.captureBinding==row?"...":values[row]);
            }
            Text(d, {c.x, c.y + 181 * s}, Fit(tr(v.capturingKey ? (v.captureBinding==15?"capturePad":"captureChord") : v.settingsTab==3?"controllerCaptureHint":v.settingsTab==2?"timeSettingsHint":v.settingsTab==1?"controlsCaptureHint":"appearanceHint"), 14 * s, 610 * s),
                 14 * s, t.muted, 0, t.textShadow);
            if(v.settingsTab==1) {
                const bool conflict=(config.actionHotkey<0 || config.actionHotkey==config.hotkey) && config.actionModifier==config.hotkeyModifier;
                Text(d,{c.x,c.y+145*s},Fit(tr(conflict?"bindingConflict":"controlsBindHint"),14*s,610*s),14*s,conflict?t.accent:t.muted);
            }
            if(v.settingsTab==2)Text(d,{c.x,c.y+10*s},Wrapped(tr("keepOpenHelp"),15*s,575*s,4),15*s,t.muted,575*s,t.textShadow);
            if(v.settingsTab==3)Text(d,{c.x,c.y+10*s},Wrapped(tr("controllerSchemeHelp"),15*s,575*s,4),15*s,t.muted,575*s,t.textShadow);
            if(v.settingsTab==3)Text(d,{c.x,c.y+105*s},Wrapped(tr("gamepadMoveHelp"),14*s,575*s,4),14*s,t.muted,575*s,t.textShadow);
            if(v.settingsTab==3)Text(d,{c.x,c.y+151*s},Fit(tr("gamepadMoveApplyHint"),13*s,575*s),13*s,t.muted);
            Button(d, c, s, v, defaultsButton, tr("defaults"));
            Button(d, c, s, v, cancelButton, tr("cancel"));
            Button(d, c, s, v, applyButton, tr("apply"), true);
            if (v.saveError)
                Text(d, {c.x, c.y + 300 * s}, Fit(tr("saveError"), 14 * s, 620 * s), 14 * s, t.accent);
        }
        std::string State(const View &v, const Item &item)
        {
            auto tr = [&](const char *key) { return Tr(v.config, key); };
            switch (item.action)
            {
            case ActionKind::FaceLightCommand:
                return tr(item.equipped ? "lightOn" : "lightOff");
            case ActionKind::FaceLightFollowers:
                return std::to_string(item.count) + " " + tr("lightPeople");
            case ActionKind::FunctionBack:
                return tr("lightBack");
            case ActionKind::FaceLightMenu:
                return tr(item.usable ? "lightOpen" : "lightUnavailableShort");
            case ActionKind::SaveOutfit:
            case ActionKind::ImportOutfits:
                return "";
            default:
                break;
            }
            if (!item.usable)
                return tr(v.functions ? "outfitUnavailable" : "inventory");
            if (item.equipped)
                return (item.magic ? "" : "x" + std::to_string(item.count) + " · ") + tr("equipped");
            return item.magic ? "" : "x" + std::to_string(item.count);
        }
        std::string ActionHint(const View &v, const Item &item)
        {
            if (!item.detail.empty())
                return item.detail;
            const char *key = "select";
            switch (item.action)
            {
            case ActionKind::Outfit:
                key = item.usable ? (v.gamepad?(v.config.gamepadCategoryButtons==1?"padUseBumpers":"padUseTriggers"):"outfitSelect") : "outfitUnavailable";
                break;
            case ActionKind::SaveOutfit:
                key = "outfitSave";
                break;
            case ActionKind::ImportOutfits:
                key = "outfitImport";
                break;
            case ActionKind::FaceLightCommand:
                key = "lightUse";
                break;
            case ActionKind::FaceLightFollowers:
            case ActionKind::FaceLightMenu:
                key = "lightOpen";
                break;
            case ActionKind::FunctionBack:
                key = "lightBack";
                break;
            default:
                key = item.usable ? (v.gamepad?"padSelect":"select") : "inventory";
                break;
            }
            return Tr(v.config, key);
        }
        bool DetailCard(ImDrawList *d, ImVec2 c, float s, const View &v, const Item &item, const std::string &state,
                        const std::string &typeTitle, float expansion)
        {
            const auto &t = Style(v.config);
            const auto viewport = ImGui::GetIO().DisplaySize;
            const float width = 278 * s, margin = 14 * s, gap = 346 * s;
            float left = c.x + gap;
            if (left + width > viewport.x - margin)
                left = c.x - 304 * s - width;
            if (left < margin || left + width > viewport.x - margin)
                return false;
            const bool hasInfo = item.info.HasData();
            const auto info = DescribeItem(item.info, [&](const char *key) { return Tr(v.config, key); });
            const int statRows = (static_cast<int>(info.stats.size()) + 1) / 2;
            const float infoHeight = statRows * 28.f + (info.heading.empty() ? 0.f : 27.f) +
                                     info.effects.size() * 42.f + (info.more.empty() ? 0.f : 20.f);
            const float height = hasInfo ? 194.f + infoHeight : 218.f;
            const float top =
                std::clamp(c.y - height * s * .5f, margin, std::max(margin, viewport.y - height * s - margin));
            const ImVec2 a{left, top}, b{left + width, top + height * s};
            const int first = d->VtxBuffer.Size;
            Surface(d, a, b, t, s, true);
            DrawIcon(d, {left + 32 * s, top + 32 * s}, item, .82f * s * t.iconScale, item.usable ? t.accent : t.muted);
            LeftText(d, {left + 56 * s, top + 25 * s}, Fit(typeTitle, 14 * s, 205 * s), 14 * s, t.muted);
            d->AddLine({left + 18 * s, top + 56 * s}, {b.x - 18 * s, top + 56 * s}, Alpha(t.border, .65f), s);
            LeftText(d, {left + 18 * s, top + 72 * s}, Wrapped(item.name, 20 * s, 242 * s, 2), 20 * s, t.text, 242 * s);
            LeftText(d, {left + 18 * s, top + 122 * s}, Fit(state, 14 * s, 242 * s), 14 * s,
                     item.equipped ? t.accent : t.muted);
            const auto hint = ActionHint(v, item);
            if (hasInfo)
            {
                float y = 150.f;
                for (std::size_t i = 0; i < info.stats.size(); ++i)
                {
                    const auto &row = info.stats[i];
                    LeftText(d, {left + (18 + (i % 2) * 126) * s, top + (y + (i / 2) * 28) * s},
                             Fit(row.label + "  " + row.value, 14 * s, 116 * s), 14 * s, t.text);
                }
                y += statRows * 28;
                if (!info.heading.empty())
                {
                    d->AddLine({left + 18 * s, top + (y + 2) * s}, {b.x - 18 * s, top + (y + 2) * s},
                               Alpha(t.border, .55f), s);
                    LeftText(d, {left + 18 * s, top + (y + 9) * s}, Fit(info.heading, 12 * s, 242 * s), 12 * s,
                             t.accent);
                    y += 27;
                }
                for (const auto &effect : info.effects)
                {
                    LeftText(d, {left + 18 * s, top + y * s}, Fit(effect.label, 14 * s, 242 * s), 14 * s, t.text);
                    LeftText(d, {left + 18 * s, top + (y + 18) * s}, Wrapped(effect.value, 12 * s, 242 * s, 2), 12 * s,
                             t.muted, 242 * s);
                    y += 42;
                }
                if (!info.more.empty())
                {
                    LeftText(d, {left + 18 * s, top + y * s}, Fit(info.more, 12 * s, 242 * s), 12 * s, t.muted);
                    y += 20;
                }
                d->AddLine({left + 18 * s, top + (y + 4) * s}, {b.x - 18 * s, top + (y + 4) * s}, Alpha(t.border, .55f),
                           s);
                LeftText(d, {left + 18 * s, top + (y + 14) * s}, Fit(hint, 12 * s, 242 * s), 12 * s,
                         item.usable ? t.muted : t.accent);
            }
            else
                LeftText(d, {left + 18 * s, top + 158 * s}, Wrapped(hint, 14 * s, 242 * s, 3), 14 * s,
                         item.usable ? t.muted : t.accent, 242 * s);
            const float reveal = SmoothPhase(expansion, .42f, 1.f);
            Reveal(d, first, reveal, {(left > c.x ? -1.f : 1.f) * 12 * s * (1 - reveal), 0});
            return true;
        }
    } // namespace
    void ResetVisualFeedback()
    {
        feedback = Feedback{};
    }
    void DrawWheel(const View &v, float opacity, float expansion)
    {
        if (!v.open)
            return;
        auto &io = ImGui::GetIO();
        auto d = ImGui::GetBackgroundDrawList();
        const int firstVertex = d->VtxBuffer.Size;
        const auto &config = v.config;
        expansion = config.animations ? std::clamp(expansion, 0.f, 1.f) : 1.f;
        const auto &t = Style(config);
        auto tr = [&](const char *key) { return Tr(config, key); };
        if (config.overlayOpacity > 0)
            d->AddRectFilled({0, 0}, io.DisplaySize, IM_COL32(0, 0, 0, config.overlayOpacity * 255 / 100));
        const float s = ViewScale(io.DisplaySize.x, io.DisplaySize.y, v);
        const ImVec2 c = ViewCentre(io.DisplaySize.x, io.DisplaySize.y, v);
        constexpr float step = 2 * std::numbers::pi_v<float> / slots;
        const float outer = 264 * s, inner = 116 * s;
        const int selected = WheelSlot(v.x, v.y);
        if (v.outfitDialog || v.settingsOpen)
        {
            ResetVisualFeedback();
            if (v.outfitDialog)
                OutfitPanel(d, c, s, v);
            else
                SettingsPanel(d, c, s, v);
        }
        else
        {
            const int category = v.functions ? 100 + int(v.functionSection) : int(v.category);
            if (feedback.category != category || feedback.page != v.page)
            {
                const bool first = feedback.category < 0;
                feedback.hover.fill(0);
                feedback.age = first ? 1.f : 0.f;
                feedback.category = category;
                feedback.page = v.page;
            }
            const float dt = std::clamp(io.DeltaTime, 0.f, .05f);
            feedback.age += dt;
            const float pageT = !config.animations || t.pageDuration <= 0
                                    ? 1.f
                                    : std::clamp(feedback.age / t.pageDuration, 0.f, 1.f),
                        pageFade = pageT * pageT * (3 - 2 * pageT);
            const auto typeTitle = v.functions
                                       ? tr(v.functionSection == FaceLight::Section::Outfits    ? "outfitTitle"
                                            : v.functionSection == FaceLight::Section::Lighting ? "lightTitle"
                                                                                                : "lightFollowers")
                                       : tr(v.visibleCategories.Count()?categoryKeys[int(v.category)]:"title");
            const int titleStart = d->VtxBuffer.Size;
            CategoryStrip(d, c, s, v, pageFade);
            Rule(d, {c.x, c.y - 283 * s}, 160 * s, t, s);
            const float frameReveal = SmoothPhase(expansion, .15f, .9f);
            Reveal(d, titleStart, frameReveal, {0, 8 * s * (1 - frameReveal)});
            const int railStart = d->VtxBuffer.Size;
            PageRail(d, c, s, v);
            Reveal(d, railStart, frameReveal, {8 * s * (1 - frameReveal), 0});
            // Keep the settled rim continuous. During the sweep each blade carries
            // its own rim and ticks, so the closed part never outlines a full wheel.
            if (expansion >= 1.f)
            {
                d->AddCircle(c, outer + 8 * s, Alpha(t.border, .8f), 192, t.borderWidth * s);
                d->AddCircle(c, outer + 12 * s, Alpha(t.border, .28f * t.ornament), 192, s);
            }
            if (expansion >= 1.f && t.ornament > 0)
                for (int i = 0; i < slots * 3; ++i)
                {
                    const float a = i * step / 3;
                    d->AddLine(At(c, outer + 8 * s, a), At(c, outer + (i % 3 == 0 ? 14.f : 11.f) * s, a),
                               Alpha(t.accent, (i % 3 == 0 ? .65f : .25f) * t.ornament), s);
                }
            for (int slot = 0; slot < slots; ++slot)
            {
                const int index = PageItemIndex(v,slot);
                const bool occupied = index>=0 && index < int(v.items.size()), hover = occupied && slot == selected;
                auto &weight = feedback.hover[slot];
                if (!config.animations || t.hoverDuration <= 0)
                    weight = hover ? 1.f : 0.f;
                else
                {
                    weight = std::lerp(weight, hover ? 1.f : 0.f, std::min(1.f, dt / t.hoverDuration * 2.5f));
                    if (std::abs(weight - (hover ? 1.f : 0.f)) < .002f)
                        weight = hover ? 1.f : 0.f;
                }
                const float blade = BladeExpansion(expansion, slot, slots);
                if (blade <= .001f)
                    continue;
                const int bladeStart = d->VtxBuffer.Size;
                const float a = slot * step, fullHalf = step * .5f - .020f, begin = a - fullHalf,
                            end = begin + 2 * fullHalf * blade, bladeAngle = (begin + end) * .5f,
                            bladeOuter = std::lerp(inner + 8 * s, outer, blade);
                if (expansion < 1.f)
                {
                    Arc(d, c, bladeOuter + 8 * s, a - step * .5f, a - step * .5f + step * blade, Alpha(t.border, .8f),
                        t.borderWidth * s);
                    Arc(d, c, bladeOuter + 12 * s, a - step * .5f, a - step * .5f + step * blade,
                        Alpha(t.border, .28f * t.ornament), s);
                    if (t.ornament > 0)
                        for (int tick = -1; tick <= 1; ++tick)
                        {
                            const float angle = a + tick * step / 3;
                            if (angle > a - step * .5f + step * blade)
                                continue;
                            d->AddLine(At(c, bladeOuter + 8 * s, angle),
                                       At(c, bladeOuter + (tick == 0 ? 14.f : 11.f) * s, angle),
                                       Alpha(t.accent, (tick == 0 ? .65f : .25f) * t.ornament), s);
                        }
                }
                Sector(d, c, inner, bladeOuter, begin, end, occupied ? Mix(t.sector, t.hover, weight * .6f) : t.empty,
                       t.relief);
                // Insets must stay inside the partial angular span near the hinge.
                Arc(d, c, bladeOuter - 1.5f * s, begin + .008f * blade, end - .008f * blade,
                    Alpha(t.border, occupied ? .65f : .23f), t.borderWidth * s);
                Arc(d, c, inner + 1.5f * s, begin + .008f * blade, end - .008f * blade, Alpha(t.border, .35f), s);
                if (weight > 0)
                {
                    Arc(d, c, bladeOuter - 3 * s, begin + .018f * blade, end - .018f * blade, Alpha(t.accent, weight),
                        2 * s);
                    Arc(d, c, inner + 3 * s, begin + .05f * blade, end - .05f * blade, Alpha(t.accent, weight * .45f),
                        s);
                }
                const int contentStart = d->VtxBuffer.Size;
                const auto at = At(c, (189 - 16 * (1 - blade)) * s, bladeAngle);
                if (occupied)
                {
                    const auto &item = v.items[index];
                    const auto state = State(v, item);
                    const auto ink = item.usable ? Mix(t.text, t.accent, weight) : t.muted;
                    const float size = 16 * s * t.labelScale;
                    DrawIcon(d, {at.x, at.y - 19 * s}, item, 1.05f * s * t.iconScale, Alpha(ink, pageFade));
                    Text(d, {at.x, at.y + 15 * s}, Fit(item.name, size, 109 * s), size,
                         Alpha(item.usable ? t.text : t.muted, pageFade), 0, t.textShadow * pageFade);
                    Text(d, {at.x, at.y + 36 * s}, Fit(state, 12 * s, 112 * s), 12 * s,
                         Alpha(item.equipped ? t.accent : t.muted, pageFade), 0, t.textShadow * pageFade);
                    if (item.equipped)
                        Diamond(d, At(c, bladeOuter - 17 * s, bladeAngle+(ValidQuickSlot(item.quickSlot)?.07f:0.f)), 3 * s, Alpha(t.accent, pageFade));
                    if(ValidQuickSlot(item.quickSlot)) {
                        const auto badge=At(c,bladeOuter-18*s,bladeAngle);
                        const float radius=9*s;
                        d->AddCircleFilled(badge,radius,t.panel|0xFF000000u,24);
                        d->AddCircle(badge,radius,Alpha(t.accent,pageFade),24,s);
                        Text(d,badge,std::to_string(item.quickSlot+1),12*s,Alpha(t.accent,pageFade));
                    }
                    if (!item.usable)
                    {
                        const auto mark = At(c, bladeOuter - 17 * s, bladeAngle-(ValidQuickSlot(item.quickSlot)?.07f:0.f));
                        d->AddLine({mark.x - 3 * s, mark.y}, {mark.x + 3 * s, mark.y}, Alpha(t.muted, pageFade),
                                   1.5f * s);
                    }
                }
                else
                    Diamond(d, at, 2 * s, Alpha(t.border, .35f));
                Reveal(d, contentStart, SmoothPhase(blade, .55f, 1.f));
                Reveal(d, bladeStart, SmoothPhase(blade, 0.f, .18f));
            }
            d->AddCircleFilled(c, inner - 9 * s, t.panel | 0xFF000000u, 128);
            d->AddCircle(c, inner - 6 * s, Alpha(t.border, .8f), 128, t.borderWidth * s);
            Arc(d, c, inner - 10 * s, -.7f, .7f, Alpha(t.accent, .3f * t.ornament), s);
            const int centreStart = d->VtxBuffer.Size;
            const int index = PageItemIndex(v,selected);
            if (selected >= 0 && index>=0 && index < int(v.items.size()))
            {
                const auto &item = v.items[index];
                const auto state = State(v, item);
                DrawIcon(d, {c.x, c.y - 44 * s}, item, 1.15f * s * t.iconScale, item.usable ? t.accent : t.muted);
                Text(d, {c.x, c.y + 4 * s}, Fit(item.name, 20 * s, 188 * s), 20 * s, t.text, 0, t.textShadow);
                const bool card = DetailCard(d, c, s, v, item, state, typeTitle, expansion);
                const auto info = !card && !item.detail.empty() ? item.detail
                                  : state.empty()               ? ActionHint(v, item)
                                                                : state;
                auto compactInfo = info;
                if (!card && item.info.HasData())
                {
                    const auto data = DescribeItem(item.info, [&](const char *key) { return tr(key); });
                    if (!data.stats.empty())
                        compactInfo = data.stats[0].label + " " + data.stats[0].value;
                    else if (!data.effects.empty())
                        compactInfo = data.effects[0].label;
                }
                Text(d, {c.x, c.y + 38 * s}, Wrapped(compactInfo, 14 * s, 172 * s, 2), 14 * s,
                     item.equipped ? t.accent : t.muted, 172 * s, t.textShadow);
            }
            else
            {
                Diamond(d, {c.x, c.y - 43 * s}, 7 * s, Alpha(t.accent, .75f));
                Text(d, {c.x, c.y}, Fit(typeTitle, 20 * s, 188 * s), 20 * s, t.text, 0, t.textShadow);
                Text(d, {c.x, c.y + 34 * s}, Fit(tr(v.inventoryLoading && !v.functions?"inventoryLoading":v.items.empty() ?
                    (v.inventoryWide && !v.functions?"inventoryEmpty":"empty") : "move"), 14 * s, 185 * s), 14 * s,
                     t.muted, 0, t.textShadow);
            }
            // Includes the card; its later reveal remains relative to this fade.
            Reveal(d, centreStart, SmoothPhase(expansion, .08f, .75f));
            const int footerStart = d->VtxBuffer.Size;
            Rule(d, {c.x, c.y + 283 * s}, 160 * s, t, s);
            Text(d, {c.x, c.y + 304 * s},
                 std::to_string(ItemCount(v)) + " " + tr(v.functions ? "functionCount" : v.inventoryWide?"inventoryCount":"count"), 16 * s, t.accent,
                 0, t.textShadow);
            if (config.showHints)
            {
                const float left = c.x - 282 * s, right = c.x + 282 * s, top = c.y + 328 * s;
                const bool quickHint=!v.functions && !v.gamepad;
                d->AddRectFilled({left, top}, {right, top + (quickHint?68:48) * s}, Alpha(t.panel, .85f), 6 * s);
                d->AddLine({left + 12 * s, top}, {right - 12 * s, top}, Alpha(t.border, .45f), s);
                Text(
                    d, {c.x, top + 14 * s},
                    Fit(tr(v.gamepad?(v.config.gamepadCategoryButtons==1?"padUseBumpers":"padUseTriggers"):v.functions ? (v.functionSection == FaceLight::Section::Outfits ? "functionUse" : "lightUse")
                                       : "compactUse"),
                        14 * s, 535 * s),
                    14 * s, t.muted, 0, t.textShadow);
                Text(d, {c.x, top + 36 * s},
                     Fit(v.gamepad?tr(v.appliedGamepadMove?
                         (config.timeMode==0?"padNavigationRight":"padNavigationMoving"):"padNavigation"):KeyLabel([&] {
                             auto x = config;
                             x.hotkey = config.switchKey;
                             return x;
                         }()) +
                             " " +
                             tr(v.functions && v.functionSection == FaceLight::Section::Followers ? "lightNavigation"
                                                                                                  : "dualWheelHint"),
                         12 * s, 535 * s),
                     12 * s, t.muted, 0, t.textShadow);
                if(quickHint)Text(d,{c.x,top+56*s},Fit(tr(v.inventoryWide?"allQuickSlotHint":"quickSlotHint"),12*s,535*s),12*s,t.muted,0,t.textShadow);
            }
            Reveal(d, footerStart, frameReveal, {0, -8 * s * (1 - frameReveal)});
        }
        const ImVec2 pointer{c.x + v.x * 224 * s, c.y + v.y * 224 * s};
        if (!v.settingsOpen && !v.outfitDialog)
        {
            d->AddCircleFilled(pointer, 4 * s, t.accent, 16);
            d->AddCircle(pointer, 7 * s, IM_COL32(0, 0, 0, 210), 16, 2 * s);
        }
        else
        {
            const ImVec2 b{pointer.x + 4 * s, pointer.y + 17 * s}, e{pointer.x + 13 * s, pointer.y + 12 * s};
            d->AddTriangleFilled(pointer, b, e, t.accent);
            d->AddTriangle(pointer, b, e, IM_COL32(0, 0, 0, 240), s);
        }
        Reveal(d, firstVertex, opacity);
    }
} // namespace Wheel

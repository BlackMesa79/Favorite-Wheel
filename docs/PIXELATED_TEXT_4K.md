# Text and category-arrow pixelation at 4K

October 13, 2026, included in 0.7.0. A user described hard pixel edges on text and the `< >` category arrows, probably at 3840×2160, while the other icons looked normal. Their exact font path, screenshot and new diagnostic log were not supplied.

## Reproduced cause

The category arrows were literal text characters in the category caption. They therefore used the same font atlas as the text; item icons use separate vector drawing.

When the configured font could not be loaded, `PrepareFonts` created ImGui's embedded ProggyClean font, then merged system fallbacks. That embedded font already contains Latin letters and `< >`, so the merge did not replace them. Its glyphs have an intentionally pixel-like design, which is particularly visible at the large native sizes used at 4K. The [Dear ImGui font documentation](https://github.com/ocornut/imgui/blob/v1.91.9b/docs/FONTS.md) describes the embedded default and its scaling limitations.

The old production drawing path reproduced the reported pattern at 3840×2160 with an intentionally missing font file: block-like English text and arrows, alongside smooth vector icons. This confirms a source defect that closely matches the report; it does not establish the reporter's exact cause without their font diagnostics.

Font selection is language-file Font, then theme Font, then main INI Display/Font. Ordinary Skyrim/SkyUI font replacers do not automatically replace these local TTF/TTC/OTF files. A valid custom file remains respected; a custom file designed with pixel-shaped glyphs can naturally retain that appearance.

## Changes

- Select an installed, readable system font as the primary when the requested file is absent or invalid, before merging remaining missing glyphs. The embedded font is now the last fallback when no system font can load.
- Draw the selected category's left/right arrows as anti-aliased vector chevrons, independent of the font.
- Bake the previously missing 13-pixel logical size at native output scale. Unbaked sizes select the next larger atlas font rather than enlarging the smallest font.
- Keep the existing linear DX11 sampling, native backbuffer sizing, subset glyph atlas and directory prewarming. Horizontal snapping is disabled for proportional glyph advances; oversampling remains 2×1 to retain the atlas budget.
- Record `Wheel font source: requested='...' resolved='...' emergency_pixel=... oversample=2x1 native_pixels=true` on atlas rebuild, next to output resolution and atlas dimensions.

## Verification

Actual Draw + DX11 WARP runs passed at 1920×1080 and 3840×2160 with a missing font. The resolved primary is `C:\Windows\Fonts\segoeui.ttf` on the development machine; Latin text uses the system font rather than ProggyClean. A separate 4K run with Georgia confirmed that valid custom fonts are still used, with missing CJK glyphs merged from fallbacks. Native sizes, glyph coverage, reuse and resize checks passed.

Compact comparison: `build/pixelation-4k-comparison.png`, 760×450, about 49 KB, crops from native 4K renders enlarged identically for inspection. Before: block-like letters/arrows. After: smooth text and vector arrows. Copies are included in the test release materials; full BMPs remain in build only.

Final 4K category-cache run: prewarmed switching across 15 categories required **zero font rebuilds**; total switch font-check time about 0.98 ms, maximum about 0.13 ms. Initial atlas preparation took about 60 ms on this machine. These are local timings, not a guarantee for every save or machine.

The user could not reproduce the original symptom in their own game and did not game-test this issue. Local rendering checks passed. Ask the original reporter to test 0.7.0. If hard edges remain, obtain the `Wheel font source` and `Native-size font atlas` log entries to check the actual font, output dimensions and emergency fallback state.

# Classic Gold redesign — 0.5.3 development

The bundled selection is now **Classic Gold**, **Skyrim** and **Skyrim Polygon**. Skyrim Ellipse, Frost, Engraved Gold and Quiet Slate have been removed from the source theme directory. Ellipse geometry and legacy etched components remain available to external theme authors.

## Design

Classic Gold keeps a warm gold accent, dark surfaces and engraved ornament. `Style=classic` selects its new component renderer independently of the wheel shape, colors and material settings:

- Warm charcoal blades with brass outer and inner rims, inset arcs and engraved radial seams. The outer rim has fewer ticks than the original Classic.
- A matching double-ring center disk with small medallion rules above and below its contents.
- Double-line detail cards, settings and outfit panels, corner brackets, matching button highlights, inputs, keycaps, page rail and bottom hint panel. Detail cards reserve 24 units of padding to keep text clear of the engraving.
- The existing inner quick-key badges and outer equipped diamonds keep their positions. The hover arrow spans outer radius minus 3–8 units; the equipped diamond spans minus 13–19, retaining a five-unit radial gap on the circular wheel.

No navigation, equipment, input, time-mode or configuration-save behavior changes. Text and icon geometry remain unscaled by the shape transformation. The built-in missing-resource fallback also uses the Classic Gold renderer and palette. Omitted or unknown `Style` in an external INI still selects legacy etched components; `Style=skyrim` retains the Skyrim renderer.

## Deployment and upgrade

The development `xmake` deployment archives only `ellipse.ini`, `frost.ini`, `engraved.ini` and `minimal.ini` from the installed Themes directory. Backups are stored in `SKSE/Plugins/FavoriteWheel/ThemeBackups/0.5.3`, which the theme loader does not scan. Existing backup names get numbered suffixes instead of being overwritten.

An untouched original Classic palette is backed up and replaced by the new `classic.ini`. The comparison accepts the original color-only file and the unchanged optional visual tokens, requires all original keys, and rejects customized values, duplicate keys and unknown non-comment content. A customized Classic and other custom theme IDs are preserved. The main `FavoriteWheel.ini` is never overwritten. An unavailable theme ID in that INI displays the new Classic Gold fallback.

For a manual package update, remove the same four retired INIs from the active Themes folder after preserving any local customizations, and install the new Classic. Copying a zip over an existing mod folder alone does not delete old files.

## October 11 local verification

- Release DLL, SettingsTests and WheelPreview compiled successfully. Settings, layout, sector mesh, wheel logic, inventory and runtime layout/metadata suites passed.
- Settings tests verify the three bundled IDs, the new Classic component style and palette, the retained Skyrim Polygon shape, fallback from unavailable retired IDs, and usability without resource files. External ellipse and legacy renderer tests remain.
- Nine production drawing-path D3D11 WARP renders passed: Classic armor details, long effect details, settings, long preset naming, 720p/60% scale at the lower-right screen edge with equipped/hover markers, a partly unfolded wheel, both retained Skyrim themes, and a custom Classic with an external material. Glyph coverage, finite coordinates, viewport fitting, dimming and opaque settled interiors were checked. The background probe was moved off the central title's snapped one-pixel shadow at tiny scales; its assertion remains enabled, and failures now report their source line.
- Classic wheel/details, settings, long naming, small-screen edge placement and partial unfolding were visually inspected. Previews are at most 1280 pixels on the long edge and below 300 KiB, under `build/classic-redesign053-*.jpg`.
- A separate deployment fixture verified retirement archival and preservation of a customized Classic, a custom theme ID, Skyrim and the main INI. The actual MO2 deployment contains exactly the three bundled theme files. The installed Classic matches the source, and all 12 recorded main-INI/language/Skyrim/material hashes remain unchanged.

Actual target: `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel`. Final DLL SHA256: `4ECBC7490C910E87525A1B99E7E0F8D81DB938395399373AD5631E22019E01B2`. A pre-change DLL and all seven installed themes are preserved in `build/classic-redesign053-backup`.

These are offline rendering and deployment checks. Restart Skyrim and select Classic Gold to judge the new design against the game scene; no in-game test or release is claimed.

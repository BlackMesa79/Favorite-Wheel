# Material skins — layer 2 (0.5.3 development)

The first stage of layer 3 now adds selectable wheel shapes and detail-position preferences, with the retained Skyrim Polygon example. See [LAYOUT_SKINS_0.5.3.md](LAYOUT_SKINS_0.5.3.md). Current bundled themes are Classic Gold, Skyrim and Skyrim Polygon. The redesigned Classic uses `Style=classic`; external legacy `etched` themes and ellipse layouts remain supported. See [CLASSIC_GOLD_REDESIGN_0.5.3.md](CLASSIC_GOLD_REDESIGN_0.5.3.md) for the current selection and upgrade behavior.

The first-layer Skyrim skin and the low-speed physics mitigation have received a successful user test. This second layer adds actual local image materials and a distinct Nordic frame treatment. The October 11 frame and detail-card refinements still need a new in-game test. The previous Double Favorite / Skyrim Souls combined compatibility tests remain outstanding.

## Visible changes

Skyrim uses an original charcoal/fibrous-grain material for wheel blades, the center disk, detail cards, settings and outfit windows, inputs and buttons. Thin silver-gray outlines surround dark, subtly shaded surfaces. Small interwoven corner glyphs replace the previous broad rim and side inlays. Narrow blade rims and angled selection markers keep attention on icons and text. Decorative diamonds along the inner blade edge have been removed following game feedback; the central item icons, empty-slot marks and outer equipped diamonds remain. Corner glyphs scale down on short panels and help strips.

### October 11 vanilla-UI reference and spacing refinement

The reference is the **first, original Skyrim inventory screenshot** (Baked Potatoes) in Becky Schmader's [Upgrading Skyrim UI](https://medium.com/@bschmader/upgrading-skyrim-ui-5b8689bc1524), published August 14, 2020. The later images in that article are redesigns and were not treated as vanilla references. The original screenshot informed the thin double frames, small angular corner ornaments, restrained monochrome hierarchy, selection arrows and space between the name, attributes and effect text. No reference images or Bethesda assets are included in the mod.

Nordic detail cards now reserve **30 layout units** at each side, beyond the 23-unit corner decoration, rather than the old 18-unit inset. Cards widen from 278 to 302 units and receive matching extra vertical space, keeping the original 242-unit body width. Plain Skyrim cards use 24 units of inset and a 290-unit width; Etched cards retain their original size. Header, rules, names, status, both stat columns, effect descriptions and action hints share the same content boundary. Existing screen-fit checks still move the card to the opposite side or use the center-detail fallback if it cannot fit. Other panel sizes, hit areas and animation timing are unchanged.

This is an independent design inspired by Skyrim menus, not a reproduction of Bethesda's SWF or textures. The existing North Etch icons, typography, layout, hit areas and animation timing remain in use. The four original themes remain untextured unless their author explicitly adds a material.

## Authoring a skin

Copy a theme INI and give it a new filename/Name, then place a PNG under its `Themes` directory. For example:

```ini
Name=My Nordic Skin
Style=skyrim
FrameStyle=nordic
SurfaceTexture=Textures/my-material.png
MaterialStrength=0.9
MaterialZoom=1
```

| Key | Meaning | Missing/invalid fallback |
| --- | --- | --- |
| `Style` | `classic` redesigned gold components, `skyrim` components, or `etched` legacy components | `etched` for omitted/unknown external values |
| `FrameStyle` | `nordic` layered borders and corner motifs within Skyrim shapes; `plain` first-layer outlines | `plain` |
| `SurfaceTexture` | PNG path relative to the theme INI directory; absolute/rooted paths, `..`, drive/stream paths and other formats are rejected | Solid surfaces |
| `MaterialStrength` | 0–1; 0 disables loading. Blends the image into a white multiplier before GPU upload | 0.85 |
| `MaterialZoom` | 1–4; centered image zoom inside each surface, no tiling | 1 |

RGBA values and the first-layer theme tokens remain available. The image multiplies those colors: white preserves the chosen color; darker pixels darken it. PNG alpha changes the **material intensity**, not surface transparency. The resulting texture is opaque, preserving readable solid backgrounds. Full-color materials are allowed; their RGB tint multiplies the theme's RGB.

`SurfaceTexture` applies to filled surfaces in all three component styles. `FrameStyle=nordic` changes borders only with `Style=skyrim`. `Ornament=0` removes ornamental corner glyphs while retaining functional framing and selection marks. Set `SurfaceTexture=` or `MaterialStrength=0` to retain geometry with flat fills. Use `FrameStyle=plain` to retain the first-layer Skyrim silhouette, with the updated detail-card inset.

No icon overrides, scriptable layouts, custom shaders, scene blur or hot-reload are introduced. Edit the skin files and restart Skyrim to reload. The image fills are clipped by each existing shape mesh rather than an image rectangle over the whole wheel; animated blades carry their own clipped material. This layer uses scalable geometric borders, not a nine-slice image frame.

## Resources and performance

Windows Imaging Component decodes PNGs during renderer initialization. D3D11 uploads a mipmapped shader resource; stable frames only look up and sample that resource. Two themes using the same path and strength share an upload. Font-atlas updates and resize operations do not recreate materials or retain swap-chain backbuffers.

Limits: PNG only, file size up to 8 MiB, each dimension 1–1024, at most 16 unique textures and 4,194,304 base-level texels total (approximately 16 MiB base RGBA storage plus mip levels). Missing/corrupt/oversized images and unavailable decoding fall back to solid fills and log a startup warning. An external image is not required to open the wheel. Decode results use an independent renderer-thread cache; failure of unrelated textures does not prevent valid skins from loading.

The shipped 512×512 material uses 262,144 texels, approximately 1.33 MiB including mipmaps. The PNG is deterministically generated by [generate_skin_material.py](../scripts/generate_skin_material.py) using only Python's standard library; it contains no third-party assets and follows the repository's GPL-3.0 license. Regenerate it with `python scripts/generate_skin_material.py`.

Texture switches add draw commands compared with the original single-atlas UI. WARP geometry and draw-command counts are useful for comparing submissions, but do not measure game FPS, GPU clocks or power. No per-frame disk reads, decodes, uploads, additional scene render targets or game-physics changes are introduced.

## Deployment and verification

Automatic deployment retains all existing theme/main INI contents. Only missing `FrameStyle`, `SurfaceTexture`, `MaterialStrength` and `MaterialZoom` keys from a bundled theme are appended. Existing values, including explicit blank/zero values, are retained. New shipped PNGs are copied only when absent, preserving a user's replacement image. The main configuration INI is not distributed in installation archives; the existing packaging script recursively includes the new runtime image directory.

SettingsTests verify old themes, style selection, path resolution/rejection and numeric fallback. SkinTextureTests use a WARP device and actual WIC decoding to verify damaged/missing/oversized fallback, opaque alpha conversion, RGBA order, mip creation, deduplicated uploads, texture budgets and cache reset. WheelPreview shares the production texture path; its optional final package-root argument supports first-layer and missing-image comparisons.

Game checks: select Skyrim and Apply, inspect materials/borders at 1440p, switch back to Classic and cancel a preview, open details/settings/outfit naming, check fan animation at small and large scales, then repeat in the Community Shaders UI layer and Slow/Normal modes. Compare frame time at the same location. Missing images should log a warning and show solid surfaces rather than disable the wheel.

### October 11 delivery checks

- Release build and automatic deployment succeeded, retaining development version 0.5.3. No release ZIP was generated.
- 24 production-path D3D11 WARP previews passed: eight languages with long effect lists, 1440p long names/armor details, 1080p small-scale Japanese and French details on opposite sides, 3440×1440 at 150% scale, Russian settings, Chinese preset naming, function and empty wheels, three intermediate animation poses and four original themes. Representative details, settings, boundary sizes, animation and Classic images were also visually inspected; decoration and content remained separated in those cases.
- SettingsTests, SkinTextureTests and WheelLogicTests passed. RuntimeLayoutTests passed six runtime fixtures, Address Library formats 1/2/5 and the built DLL metadata; this is not six in-game runtime tests.
- All 16 recorded installation INI/material/meta files retained their original SHA256. The installed DLL matches the build: `0BA402D9FEC6AFB0CE6CE635D3FD69905769D367233E8D0A60654E7492BE09D6`.
- Local records: `build/vanilla053-build.txt`, `build/vanilla053-previews.txt`, `build/vanilla053-tests.txt` and `build/vanilla053-preservation-check.json`. Previous DLL backup: `build/FavoriteWheel-before-vanilla053.dll`.

WARP checks confirm offline layout and rendering behavior, not game FPS or GPU power. The new visual treatment still needs the user's in-game review, including real item names/effects and the Community Shaders layer. No gameplay, time-mode or input behavior changed in this refinement.

### Follow-up: blade ornament and status alignment

The user confirmed the preceding skin treatment looked good in game. Inner-edge decorative diamonds are removed. After further in-game feedback, quick-slot numbers now sit fifteen units beyond the sector's inner edge; the inset keeps the nine-unit keycap's corners within diagonal blades. Functional equipped diamonds sit sixteen units inside the outer edge, centered on the radial midline. Skyrim's hover arrow is centered again: it occupies three to eight units inside the outer edge, while the diamond's outermost point is thirteen units inside it, leaving a five-unit geometric gap. Empty-slot markers and navigation ornaments retain their respective functions.

The blade status row is explicitly anchored to the same horizontal center as its icon and item name, including diagonal blades. This shared drawing path covers all five themes, inventory/favorite items, spells/shouts/powers, outfit presets and lighting actions, including fan animation poses. Counts and equipped/on/off text remain one centered status row. The revised positions passed five production-path WARP previews, with visual checks of top and lower-diagonal hover at 1440p, bottom hover at 720p/60% scale, Classic at 1080p and a partial fan pose. These are offline checks; the revised placement still needs in-game confirmation. No equipment or input behavior changes.

The associated animation performance work replaces general sector triangulation and skips invisible drawing work; measurements and verification are recorded in [ANIMATION_PERFORMANCE_0.5.3.md](ANIMATION_PERFORMANCE_0.5.3.md).

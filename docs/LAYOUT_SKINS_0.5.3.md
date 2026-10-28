# Layout skins — layer 3, first stage (0.5.3 development)

This stage adds wheel shapes and detail placement to theme INIs. Categories, paging, wheel switching, button bindings and the ten-item wheel structure keep their existing behavior. The user confirmed the preceding torch, paused equipment, poison use and separate poison category changes; the user also confirmed the first layout stage. The subsequent Classic Gold redesign still needs its own in-game check.

## Try the bundled examples

Restart Skyrim, open the wheel, then use Settings → Appearance → Theme:

- **Classic Gold**: the redesigned circular brass wheel with automatic detail placement.
- **Skyrim**: the circular Nordic material skin with automatic detail placement.
- **Skyrim Polygon**: a regular ten-sided wheel with preferred right-side details.

Only these three themes are bundled. Skyrim Ellipse, Frost, Engraved Gold and Quiet Slate are retired; ellipse geometry remains available to external theme authors. Deployment archives the four retired INIs outside the theme directory and upgrades an untouched original Classic. Custom theme IDs and the main INI are preserved; an unavailable selected theme displays Classic Gold.

## Theme authoring

Copy a theme INI to a new filename under `SKSE/Plugins/FavoriteWheel/Themes`, then add layout keys. They are independent of `Style`, `FrameStyle`, colors and materials, so all three component styles can use them.

```ini
Name=My Layout
Style=skyrim
WheelShape=ellipse
WheelStretchX=1.2
WheelStretchY=0.94
DetailPosition=left
DetailGap=20
```

| Key | Values | Default and behavior |
| --- | --- | --- |
| `WheelShape` | `circle`, `ellipse`, `polygon` | `circle`; polygon has ten faces, one per item direction |
| `WheelStretchX` | 0.9–1.25 | 1.2; horizontal stretch, used only for ellipse |
| `WheelStretchY` | 0.9–1.2 | 0.94; vertical stretch, used only for ellipse |
| `DetailPosition` | `auto`, `left`, `right`, `top`, `bottom`, `center` | `auto`; positions are preferences, with a fit check |
| `DetailGap` | 16–80 | 20 design units; clearance from the existing UI |

Tokens accept surrounding whitespace and mixed case. Unknown shape/position values fall back to circle/auto. Invalid or nonfinite numbers keep defaults; finite numbers are clamped to the documented ranges. These keys belong to the selected theme, with no new main-INI or save-game fields.

## Placement and selection

Sector surfaces, inner/outer outlines, item anchors, hover arrows and equipped/quick-slot markers follow the shape. Text, icons and keycaps retain their normal proportions; narrow elliptical layouts reduce available label width rather than squeeze glyphs. Material coordinates use the shaped bounds.

Rendering, inventory selection, detail capture, quick-slot assignment and pointer clamping share the same shape calculation. The virtual mouse cursor and controller rays therefore address the displayed sectors. Slot order, the neutral selection zone and ten equal angular directions remain; circular pointer behavior is retained. A polygon's flat faces are centred on the item directions, with corners between sectors.

Larger ellipses reserve room in screen fitting and move the existing top strip, right page rail and bottom hints outward. Their navigation structure is unchanged. Settings and outfit dialogs retain their existing centred layout and hit regions.

`auto` tries right, left, top and bottom. A specified position is tried first, then available sides in that order. A full card must fit within the screen margin and avoid the wheel, category strip, page rail and hints. Upper/lower cards need sufficient vertical space; reducing the wheel scale or moving its screen position can make that space available. Oversized or blocked cards fall back to the central compact summary. `center` explicitly uses that compact summary, without an external card. The card's appearance motion stays within its clearance from navigation.

First-stage scope is these shape and position presets. Arbitrary meshes, slot counts, rotated slot order, freely draggable cards, replacement navigation hierarchies and theme scripts remain future extensions.

## Local verification

Build/test records are under `build/layout-skin053-*`:

- `LayoutSkinTests`: 1,350 sector, outside/neutral, mouse-clamping and controller cases; 13,122 viewport/card cases across three shapes, six placement modes, screen sizes and card heights. Checks preferred placement, opposite-side fallback, compact fallback, polygon flatness/corners and legacy circular motion/card coordinates.
- `SettingsTests`: bundled examples, mixed-case/whitespace tokens, bounds, unknown/invalid/nonfinite fallback and existing persistence/localization behavior.
- `SectorMeshTests`: 1,200 AA/non-AA poses across all three shapes; optimized strip fills match the reference boundary and signed area, including index rollover and tiny poses.
- Existing wheel logic, inventory and runtime layout/format/DLL metadata tests pass. No new game hooks or relocation IDs were introduced.
- Actual WARP rendering covers the two examples, legacy circle, long effect cards, vertical stretch limits, upper/lower cards, explicit compact centre, small-screen edge placement and a partly expanded ellipse. Screen fitting, glyph coverage, finite coordinates and opaque settled sectors are checked. JPEG previews have a maximum 1280-pixel long edge and are under 300 KB.

Visual inspection found no new overlap in the two example shapes, upper card, narrow/tall ellipse and partly expanded ellipse. These are local geometry and rendering checks; they do not establish game FPS or actual device input behavior.

The final DLL is deployed to `H:/Games/Dev Skyrim/mods/36 - FavoriteWheel/SKSE/Plugins/FavoriteWheel.dll`. Its SHA256 matches the build: `520E48ECFBF2FCD87F31FFA1C5C940AE6EC104427E4D2A2DD72F71519C2AF804`. All 15 pre-existing INI/PNG files retain their hashes; the two new example themes match the source files. The previous DLL is backed up at `build/FavoriteWheel-before-layout-skin053.dll`. Final rendering also checks a top card with minimum clearance during reveal and a near-closed polygon pose; whitespace validation passes. Version remains 0.5.3 development, with no release upload or Git push.

## In-game checks

1. Select each new example and check all ten directions with mouse and controller; hover, use and 1–8 binding should address the same item. Release the stick before confirming, and verify the neutral centre.
2. Equip/unequip, use poisons, switch categories/pages and return from functions. KeepOpen and Pause/Slow/Normal must retain the previously verified behavior.
3. Try wheel scales 0.6–1.5, screen positions near the edges, long item names/effects and 720p/1440p/ultrawide displays. Cards should use an available side or compact fallback.
4. Set `DetailPosition=top`, `bottom` and `center` in a copied theme, restart, and check preferred positions when there is room, fallback when there is not, and opening/closing animation.
5. Switch back to an existing theme; settings/outfit dialogs, navigation and saved configuration should remain familiar.

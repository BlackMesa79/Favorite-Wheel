> 后续版本更新：现已支持主题引用自定义图标包和法术学派图标，见 [图标库文档](ICON_LIBRARY.md)。以下为 0.6.0 历史说明。

# Making themes for Favorite Wheel 0.6.0

The bundled themes are **Classic Gold**, **Skyrim** and **Skyrim Polygon**. You can mix component style, colors, materials and wheel layout in a separate theme INI. No plugin compilation is needed.

## Create and select a theme

1. Open `Data/SKSE/Plugins/FavoriteWheel/Themes/` in your mod's files.
2. Copy `classic.ini`, `skyrim.ini` or `polygon.ini` to a new filename, such as `my-theme.ini`. The filename without `.ini` is the theme ID; `Name` is its label in settings.
3. Edit the copied values and save as UTF-8. Use one `Key=Value` per line. Lines starting with `;` or `#` are comments. Replace existing keys rather than adding duplicates.
4. Restart Skyrim, open the wheel, and select **F2 → Appearance → Theme → Apply**. Settings are saved separately in the player's main INI.

Edits are loaded at startup; there is no hot reload. Keep a unique filename and a backup of your work. In MO2, edit the theme in the mod that supplies the winning file.

## Layer 1: colors and component style

`Style=classic` selects the redesigned Classic Gold components: brass blade rims and engraved seams, a double-ring center, and matching framed panels/buttons. `Style=skyrim` selects the Skyrim components. `Style=etched` selects the legacy renderer for external themes. Omitted or unknown Style also selects etched; `classic.ini` explicitly requests classic.

Colors use eight hexadecimal digits in **RRGGBBAA** order. For example, `Accent=EFD69CFF` is a warm gold. Main color keys are `Accent`, `Text`, `Muted`, `Sector`, `Empty`, `Hover`, `Panel` and `Border`. Settled surface fills are opaque for readability. `Background` does not set scene dimming: use the wheel's Appearance → Background dimming setting instead.

These visual tokens are independent of game controls:

| Key | Allowed range / meaning |
| --- | --- |
| `BorderWidth` | 0.5–2; frame line width |
| `CornerRadius` | 0–16; used by classic/etched panels |
| `Ornament` | 0–1; decorative engraving strength |
| `Relief` | 0–1; blade shading where supported |
| `TextShadow` | 0–1; text shadow strength |
| `IconScale` | 0.8–1.15 |
| `TitleScale` | 0.85–1.15 |
| `LabelScale` | 0.9–1.1 |
| `HoverDuration` | 0–0.25 seconds; 0 is immediate |
| `PageDuration` | 0–0.25 seconds; 0 is immediate |

`Font=` can stay empty to use the language/main font choice, or specify a locally installed font path. Fonts are not redistributed. The wheel's animation toggle controls local transitions as well as its fan animation; opening/closing duration is not a theme key.

## Layer 2: local PNG materials and frames

Put a PNG in `Themes/Textures/` and reference it from your INI:

```ini
SurfaceTexture=Textures/my-material.png
MaterialStrength=0.85
MaterialZoom=1
```

`SurfaceTexture` is relative to the theme's directory. Absolute paths, rooted/drive paths, `..`, alternate streams and non-PNG formats are rejected. Use at most 1024×1024 pixels and 8 MiB per PNG. The renderer has a shared budget of 16 material variants and 4,194,304 texels. Empty, unavailable, invalid or over-budget images fall back to solid fills and produce diagnostics.

The image multiplies theme colors: white preserves a color, and darker pixels darken it. Full-color images also tint the base color. PNG alpha controls material intensity, not see-through surfaces. `MaterialStrength` is 0–1 (default 0.85); 0 disables loading. `MaterialZoom` is 1–4 (default 1) and zooms around the center without tiling. Materials can be used with classic, skyrim or etched components.

For `Style=skyrim`, `FrameStyle=nordic` adds layered silver-gray borders and small Nordic corner ornaments. `FrameStyle=plain` uses simpler Skyrim framing. This setting does not change classic or etched components. The supplied charcoal PNG and geometric decorations are original project assets; no Bethesda artwork or Windows fonts are bundled.

## Layer 3: wheel shape and detail placement

| Key | Values / limits | Missing or invalid fallback |
| --- | --- | --- |
| `WheelShape` | `circle`, `ellipse`, `polygon` | `circle` |
| `WheelStretchX` | 0.9–1.25; ellipse only | 1.2 |
| `WheelStretchY` | 0.9–1.2; ellipse only | 0.94 |
| `DetailPosition` | `auto`, `left`, `right`, `top`, `bottom`, `center` | `auto` |
| `DetailGap` | 16–80 design units | 20 |

Polygon is a regular ten-sided wheel, with one face per item direction. Ellipse changes the wheel geometry; text, icons and key badges retain their normal proportions. Shape-aware drawing, pointer limits and mouse/controller selection share the same geometry.

Detail positions are **preferences**. The card avoids the wheel, category strip, page rail and hints and can choose another position if the requested side does not fit. If no full card fits, the center displays compact information. `center` directly selects that compact display rather than putting a full card on top of the wheel.

Shape/position names accept mixed case and surrounding whitespace. Invalid or nonfinite numeric tokens keep defaults; finite out-of-range values are clamped. The wheel remains ten slots per page with its existing categories, navigation and bindings. Custom navigation hierarchies, arbitrary meshes, slot counts, icon replacements and scriptable layouts are not supported by this release. Settings and outfit dialogs keep their centered layouts.

## Complete example

Save this as `Themes/my-gold-polygon.ini`. It uses Classic Gold components with a ten-sided wheel and left-side card preference; it needs no additional image.

```ini
Name=My Gold Polygon
Style=classic
Font=
Accent=EFD69CFF
Text=F2E9E0FF
Muted=B9ADA1FF
Sector=262322FF
Empty=1C1917FF
Hover=604A34FF
Panel=211C19FF
Border=9C7B59FF
BorderWidth=1
CornerRadius=6
Ornament=0.8
Relief=0.65
TextShadow=0.36
IconScale=1
TitleScale=1
LabelScale=1
HoverDuration=0.10
PageDuration=0.12
SurfaceTexture=
MaterialStrength=0.85
MaterialZoom=1
WheelShape=polygon
DetailPosition=left
DetailGap=24
```

To try a textured Nordic variant, set `Style=skyrim`, add `FrameStyle=nordic`, and set `SurfaceTexture=Textures/skyrim-charcoal.png`. To try an ellipse, set `WheelShape=ellipse`, `WheelStretchX=1.15` and `WheelStretchY=0.95`.

Test your theme with long item names, equipped items plus quick keys and hover arrows, long effect cards, small screens, screen-edge placement, settings, outfits and partial fan animation. A shape preference can fall back at small scales or extreme positions.

## Share a theme and upgrade older installations

Share your uniquely named INI under `SKSE/Plugins/FavoriteWheel/Themes/` and any referenced PNGs under that theme's relative texture path. Include appropriate asset permissions. Leave out `FavoriteWheel.dll` and the player's main `FavoriteWheel.ini`; a theme does not need to replace them.

0.6.0 retires the bundled `frost.ini`, `engraved.ini`, `minimal.ini` and the development `ellipse.ini`. Back up local edits before removing those four files from an older installed Themes folder. Install the new `classic.ini`; if you want to keep a customized old Classic, first save it under a new filename and display name. A ZIP copied over an existing directory does not remove retired files. Other custom theme IDs and the main settings INI can be kept. An unavailable selected theme displays Classic Gold.

The source checkout's development deployment archives those four retired filenames under `FavoriteWheel/ThemeBackups/0.6.0` and upgrades an untouched original Classic. This is a build/deployment helper; the installation ZIP does not execute that migration.

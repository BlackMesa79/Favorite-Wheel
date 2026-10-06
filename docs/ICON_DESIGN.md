# North Etch icon family

The wheel uses 29 original, code-drawn outline pictograms in `include/WheelIcons.h`: 22 item types, three outfit actions, three lighting controls and a back arrow. `Auto` resolves to the existing category fallback. No external icon pack or raster texture is required.

The design combines softly curved silhouettes with angular tips and restrained engraving. The sword is upright; the dagger has a short, swept diagonal blade. Armor uses a tapered breastplate, while the shield has a broader curved perimeter. Rounded glass, fruit, leather and flame shapes contrast with metal edges. Small elongated diamonds repeat in jewelry, the staff, the potion label and magic.

- Coordinates are centred on the origin in a 40 × 40 design box.
- Main contours use 1.75 × scale strokes; secondary engraving uses 1.15 × scale.
- All paths are outlines, including gems. Color and opacity come from the caller so the icons follow theme, selection and disabled states.
- Curved silhouettes use continuous ImGui paths. Icon rendering only reads item value data and never queries game objects.
- Save outfit uses clothing with a plus. Player lighting uses rays, target lighting uses framing corners, and group lighting uses three heads.
- Blade, guard and grip share exact attachment points; the dagger's inner curve follows its swept blade. The axe handle is closed, the mace has a broad flanged head, and the arrow has matched feather vanes. The shield uses an inset rim and round boss. The potion omits the stray highlight; the pouch has a continuous closed silhouette. The hanger's circular hook ends in a centred vertical stem connected to both shoulders.

## Visual verification

Build `WheelPreview`, then run from the repository root:

```powershell
& './build/windows/x64/release/WheelPreview.exe' build/icons-north-1440.bmp 2560 1440 icons zh_CN classic
& './build/windows/x64/release/WheelPreview.exe' build/icons-north-720.bmp 1280 720 icons en frost
```

The `icons` mode draws all 29 glyphs beside the actual wheel with synthetic inventory. The contact sheet is preview-only and does not appear in the game. Review both the contact sheet and the smaller wheel/detail icons. This is D3D11 WARP offscreen verification, not an in-game test.

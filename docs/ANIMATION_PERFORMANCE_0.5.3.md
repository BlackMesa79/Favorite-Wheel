# Animation performance investigation (0.5.3 development)

October 11, 2026: users report that opening the wheel stutters with animations enabled and improves when animations are disabled. This comparison identifies an affected path, but offline rendering cannot establish the in-game cause or confirm that the hitch is resolved.

## Changes

- Replace general concave-polygon ear clipping with a linear annular strip for visible wheel sectors. Nordic Skyrim sectors have two fills per blade, so both benefit. Preserve boundary vertices, antialiasing, vertex shading and material mapping. Subpixel arc segments in nearly folded blades retain the legacy fill to preserve their rasterization.
- Skip building blade icons/text/badges and detail cards while their reveal is completely transparent.
- Skip vertex traversal when opacity and displacement require no changes.
- Log one CPU timing summary per completed, reversed or cancelled opening/closing transition: animation setting, frame count, maximum/mean CPU render time, font rebuild count/time and peak geometry. No per-frame logging or GPU synchronization is added.

The 220 ms transition, fan pose mapping, font pixel sizes, animation toggle, input hit regions and immediate menu/time release remain unchanged. Closing still draws a cached view after the menu closes and actions become eligible; animation does not delay item use.

## Offline measurements

`WheelPreview` accepts `benchmark` at argv[14], after the package root. It measures only `DrawWheel` and `ImGui::Render`, excluding font preparation, WARP submission/readback and the game. Each pass warms 60 frames then measures 1,200 frames. A sweep repeatedly samples progress 0–1 with a fixed selection; these poses represent either transition direction.

Three interleaved before/after passes used Chinese, Skyrim materials, ten equipped items/quick slots and a wheel fitted to a 2560×1440 viewport. These are medians of the three per-pass measurements:

| Drawing path | Before mean | After mean | Before p95 | After p95 |
| --- | ---: | ---: | ---: | ---: |
| Animation sweep | 140.15 µs | 74.54 µs | 257.10 µs | 112.60 µs |
| Settled wheel | 241.30 µs | 117.53 µs | 275.60 µs | 151.90 µs |

This is about 47% less CPU drawing time during the synthetic sweep and 51% less while settled. Peak geometry remains 15,270 vertices / 60,075 indices / 27 draw commands during the sweep, versus 14,642 / 57,429 / 27 when settled. The sweep is cheaper on average because fewer blades are visible. These sub-millisecond CPU measurements do **not** explain a large game hitch by themselves or measure GPU cost. Do not present them as an FPS improvement.

The backend can grow vertex/index buffers as the first sweep gains geometry; font-atlas rebuilding also invalidates backend device objects. Neither occurs on every warmed frame, and font baking is independent of expansion. Actual timing/logs are needed before identifying these as causes. Closing additionally overlaps cached UI drawing with resumed gameplay; compare opening and closing separately.

## Verification

- `SectorMeshTests`: 400 poses spanning ten orientations, five progress values, four scales and AA on/off. Boundary positions/colors/UVs match the legacy fill exactly, signed interior areas and index bounds match, and 16-bit vertex-offset rollover/empty fills pass.
- Sixteen paired production-path WARP previews: equipped powers, all equipped blades, 720p/1440p/4K viewport limits, long Russian details, Chinese settings, animations disabled, all five themes, and six intermediate poses. Final raster comparisons differ by at most one channel value. Four further previews cover actual 60%, 100% and 150% wheel scales. Representative alignment and animation images were visually reviewed.
- Settings, wheel logic, skin textures and six runtime-layout fixtures/DLL metadata checks pass. These are offline tests, not six in-game runtime tests.
- Records: `build/animation053-benchmarks.json`, `build/animation053-image-comparisons.json`, `build/animation053-previews.txt`, `build/animation053-build.txt` and `build/animation053-runtime-tests.txt`. Previous DLL backup: `build/FavoriteWheel-before-animation053.dll`.

## In-game follow-up

At the same location, resolution, time mode and FPS cap, compare animations on/off after warming fonts, then compare the first opening after launch. Repeat quick open/close reversal and a normal close that resumes gameplay. `FavoriteWheel.log` now includes `Wheel transition` summaries; `font_builds` and `font_max_ms` separate font work from total CPU rendering. `cpu_max_ms` includes snapshot/font preparation, draw construction, backend submission and state restoration, but is not GPU execution time or the whole game frame. GPU capture/frame-time evidence is still required if CPU summaries stay low while the game stutters.

No animation preference or player INI is changed automatically. Development version remains 0.5.3; no release package is generated.

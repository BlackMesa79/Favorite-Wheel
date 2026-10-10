# 1080 Ti opening-wheel report

2026-10-10: a user says opening the wheel sends a Pascal GTX 1080 Ti into “overdrive” and speculates that overdraw is responsible. No FPS, GPU utilization, clock/power/temperature measurements, resolution, wheel version, time mode, renderer mods or limiter configuration were supplied. This is not enough to determine whether load, fan speed, clocks, or frame pacing is abnormal. No hardware-specific diagnosis is claimed.

## Confirmed code behavior

The renderer hooks the existing game swap-chain Present and calls the previous function exactly once with its original interval/flags. It neither disables VSync nor changes an FPS cap, and has no independent render loop. Closed wheels skip drawing after the short closing animation. Pause/slow time affect simulation, not the game renderer's frame rate.

The UI has no background blur, scene copies, compute shaders or extra scene rendering. Optional darkening is one full-screen alpha-blended rectangle; wheel shapes, antialiasing, text and text shadows add local blending. This is real additional pixel work, but not evidence of excessive overdraw. Font atlas work happens when size/font/glyph requirements change; the stable atlas is reused. Dynamic vertex/index buffers are mapped for the UI draw, and grow when needed rather than being recreated on every stable frame. The engine UI render target is used when available; the fallback creates an RTV for the existing swap-chain texture each drawn frame, without creating another scene texture.

The 1920x1080 synthetic armor-details preview before the multilingual font changes used one command list, one draw command, 8,136 vertices and 8,100 triangles. These counts describe the supplied scene only; one draw command can still contain overlapping triangles. WARP preview is CPU software rendering, not a 1080 Ti benchmark, and gives no real GPU time, clock, power or game-scene measurements.

## Working hypotheses and shortest checks

1. Compare gameplay, vanilla Favorites, and the wheel at the same location. Record FPS, GPU utilization, clock and power before/after. In Pause mode reduced CPU simulation work can allow higher rendering FPS on an uncapped configuration, increasing GPU load. A custom menu may also be treated differently by per-menu limiter settings. These are hypotheses, not confirmed causes.
2. Apply a known working cap such as 60 FPS through the user's existing limiter/driver and repeat. If load normalizes when FPS no longer rises, prioritize frame-rate/limiter policy rather than changing UI geometry. Do not automatically force a plugin-side limiter or sleep in the shared Present hook.
3. At the same cap, set background dimming to 0 and compare sustained load. Then disable open/close animations. Dimming affects sustained fill work; a 220ms transition cannot explain a continuous load increase by itself.
4. Provide the wheel version, resolution, Pause/Slow/Normal mode, ENB/Community Shaders/upscaler and limiter settings, plus FavoriteWheel.log. Repeated `Native-size font atlas` messages while stationary would point to atlas churn; a few on language/layout changes or first-time glyph discovery are expected.

If GPU time/power rises significantly at the same FPS, further investigate with measurements on the affected hardware and rendering setup. No runtime renderer/limiter changes were made solely from this report. The preview tool now reports actual UI geometry for repeatable inspection.

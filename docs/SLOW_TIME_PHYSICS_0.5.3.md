# Slow-time physics mitigation — 0.5.3 development

## Report and evidence

The user reported hanging shop/blacksmith signs twitching or flipping upward whenever the wheel opened at 20% speed. At the same location, the user tested 50% and reported normal behavior. The log confirmed transitions between global speed 1.0 and 0.2. This narrows the reproduction to low-speed operation; it does not establish the precise Havok failure or rule out transition timing as a contributor.

The installed SSE Display Tweaks configuration has dynamic Havok step scaling enabled. Its public source calculates maximum time values from the unscaled frame interval. Skyrim Souls RE's public settings source explicitly warns of possible physics problems below approximately 0.25–0.3. These are supporting observations, not proof that either plugin caused this report.

## Conservative change

- Optional Slow time now defaults to 50%, selectable from 50–100%. Pause remains the default time mode.
- Existing values below 50% normalize in memory to 50%, so the settings display matches the effective choice. Startup never rewrites the player's INI; only Apply saves the normalized value.
- Before applying relative slowdown, the wheel checks both current and target global multipliers. The factor is limited so neither falls below 0.5. A baseline of 0.75 and a 50% request therefore uses a factor of about 0.667, resulting in 0.5.
- If either baseline value is already at or below 0.5, the wheel does not acquire a time lease or write time. Existing Slow Time spells/other mods remain unchanged and are not accelerated. If an external time change happens during an owned lease, the existing relinquish-and-close behavior still preserves that external change.
- The gameplay settings footer explains the range in all eight bundled languages. A new key is appended during deployment, preserving custom language/theme values.
- No new hooks, runtime addresses, dependencies, native timer flag changes or Havok INI writes. No modifications to Display Tweaks, Engine Fixes or SMP/CBPC settings.

This intentionally restricts the previous 5–49% choices. It is a mitigation using the user's successful 50% control, **not a fix for low-speed Havok itself or a guarantee that 50% is safe in every configuration**. Keeping genuine 20% slowdown would require a separate investigation of the physics update path and interaction with timestep patches; changing those globally in this hotfix would introduce additional compatibility risk.

## Review and verification

Time-policy regression coverage checks the reported 20% request, both directions of native interpolation, pre-existing low time, 100%, invalid inputs, external changes and repeated restore cycles. Settings coverage checks legacy 20% loading without file mutation, clamped editing, Cancel, explicit Apply and reload. The low factors in the pre-existing `TimeLease` tests still exercise the generic relative ownership primitive; runtime calls are guarded by `WheelSlowFactor` before acquiring that primitive.

Game verification remains necessary: repeat opening/closing near the same signs with the old INI (effective 50%), then 75% and 100%; also check Pause/Normal, an existing Slow Time effect, settings/dialog pauses and normal restoration. The user's earlier 50% result applies to the previous build and is not a test of this new DLL. The Double Favorite / Skyrim Souls compatibility changes from the previous development build remain pending combined game tests.

Local validation on 2026-10-10: Release build and automatic deployment completed; TimeTests, SettingsTests, WheelLogicTests and RuntimeLayoutTests passed. The runtime check covers six supported layout fixtures and DLL metadata, not in-game tests on six executables. Three 720p WARP settings renders (Chinese, English, Russian) were checked visually for the displayed 50% value and complete footer; font checks cover all eight bundled languages. The 15 installed INIs/meta retain their original bytes, with only the new footer key appended to each language file. Main settings and all theme files are unchanged. Built and deployed DLL SHA256: `57ADB192FCA17E50A8530CEF4A658D206510B3379C1569F3EC12E51ED38AFD61`. Backup: `build/FavoriteWheel-before-physics053.dll`; verification artifacts: `build/physics053-*.txt`, `build/physics053-preservation-check.json`.

## Inspected public sources

- [Skyrim Souls RE settings](https://github.com/Vermunds/SkyrimSoulsRE/blob/2cbd7d5a0e2a6a04381b4076ac73e8d51332fe32/src/Settings.cpp): low-factor warning accompanying `fSlowMotionMultiplier`.
- [SSE Display Tweaks](https://github.com/SlavicPotato/SSEDisplayTweaks): `SSETweaks/havok.cpp`, `CalculateHavokValues` (public source reviewed; not asserted identical to the installed DLL).
- [Wheeler Refined](https://github.com/c0kadam/Wheeler-Refined): inspected only as a time-control reference. No implementation was copied, and its setter usage does not establish a solution for this sign regression.

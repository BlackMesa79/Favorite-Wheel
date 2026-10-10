# Compatibility review — 0.5.3 development

Reviewed 2026-10-10 using the current Favorite Wheel source, Nexus descriptions and the public source repositories listed below. This is a source review and local regression validation, **not an in-game certification of these combinations**. No third-party plugin code or assets were added to Favorite Wheel.

| Mod | Finding | Current behavior / action |
| --- | --- | --- |
| [STB Hotkey System](https://www.nexusmods.com/skyrimspecialedition/mods/191157) 1.6 | Partial coexistence; native quick-slot assignment is incompatible with STB's replacement hotkey system. | Assign STB bindings in Inventory/Magic or the original Favorites menu. Favorite Wheel does not read, display or create STB bindings. Avoid assigning native 1–8 slots in the wheel while STB is installed. Use different entrance keys/chords from STB gameplay bindings. |
| [Double Favorite As Important](https://www.nexusmods.com/skyrimspecialedition/mods/161516) 1.0.3 | Important items were incorrectly included as ordinary favorites. | Fixed in this development build: Important items are omitted from favorites mode, remain available as non-favorites in all-inventory mode, and cannot have their protection marker overwritten by wheel quick-slot assignment. Red-star drawing / marking still belongs to the original inventory UI. |
| [Skyrim Souls RE](https://www.nexusmods.com/skyrimspecialedition/mods/27859) 3.2.0 | Tween navigation, skills and level-up menus were missing from the explicit opening gate. | Fixed: TweenMenu, StatsMenu and LevelUp Menu block both wheel entrances even when unpaused. The queued opening and live-menu cancellation paths share the same gate. Favorite Wheel still controls its own Pause/Slow/Normal behavior. |

## STB hotkey boundary

STB hooks FavoritesHandler to disable the native numeric equipment path. It stores its own bindings in its SKSE co-save; its default migration imports native slots 1–8 on loading a save and clears the native slots while preserving the favorite flag. Disabling migration does **not** restore the native handler.

Favorite Wheel currently assigns `ExtraHotkey` / `MagicFavorites::hotkeys` native slots. Thus assigning 1–8 in the wheel with STB loaded can show a badge that does not control what that number equips; a later reload can migrate or retire it. This should not be described as complete hotkey compatibility.

STB's v1 public API offers Resolve, EquipNow, GetHotkey and GetHotkeyExact. It can support reading and displaying bindings, but has no assignment/deletion API. Fully replacing the wheel's native assignment requires an upstream write interface or a deliberately separate integration design. No STB API header or implementation was incorporated in this review.

Favorite Wheel removes captured wheel buttons from the downstream event chain and restores the original links after dispatch. STB's registered input sink therefore receives ordinary gameplay keys while the wheel is closed, and does not receive new captured navigation/selection keys. This is a structural expectation requiring a combined runtime test, especially with additional input hooks or STB bindings sharing Q/Shift+Q. The intentionally allowed movement and IME keys are not globally suppressed.

## Important-item boundary

Double Favorite As Important retains an `ExtraHotkey` record with slot value `0xFA` to preserve protection in other mods. Its Favorites-menu hook hides that record; Favorite Wheel reads inventory directly and previously bypassed this hiding rule.

The new check is conditional on loaded `DoubleFavoriteAsImportant.dll` and caches module presence at the first in-game inventory collection. Only that marker changes favorite semantics; unknown extension values and native unbound favorites remain unchanged. Count partitioning still happens before filtering, so hidden copies do not inflate visible stack counts. Exact-instance revalidation uses the same semantics, including the favorite-only guard before any quick-slot mutation. All-inventory mode intentionally includes Important possessions; this does not bypass vendor/enchanting protection because those menus remain under the provider's control.

No marker changes, new saved fields, new input hooks, or per-frame plugin/file scans are introduced.

## Skyrim Souls menu boundary

GameIsPaused is insufficient when a menu mod removes pause flags and leaves gameplay input enabled. The common gate now explicitly includes TweenMenu (Tab navigation hub), StatsMenu (skills/perks), and LevelUp Menu, in addition to the existing inventory/magic/dialogue/etc. menu gates. Blocking an idle entrance leaves its down/hold/up events for the native menu. A queued request is checked again before opening, and an already open wheel closes when a blocking menu appears.

The inspected Souls implementation manages configured menus and their slowdown flags; Favorite Wheel's named overlay and pause guard are not in its built-in menu configuration. Souls should therefore not directly choose the wheel's time mode. Both plugins nevertheless use the global time multipliers. A third-party time change during a wheel session makes Favorite Wheel close and preserve the external values instead of overwriting them; this is a safeguard, not a guarantee that arbitrary stacked time mods combine correctly.

## Sources

- [STB source](https://github.com/STB-Team/STB-Hotkey-System/tree/e1c6d54fa744f12b4df3e633b4d07b9daf3d874c): `src/InputHandler.cpp`, `FavoritesHook.cpp`, `VanillaMigration.cpp`, and `api/README.md` / `STB_HotkeySystemAPI.h`.
- [Double Favorite source](https://github.com/JerryYOJ/DoubleFavoriteAsImportant-SKSE/tree/67a3ce7e829bc0d9fd5e3c6518845b4315e8528d): `src/hooks/HookFavorite/HookFavorite.cpp`, `HookHideItems/HookHideItems.cpp`, and `src/plugin.cpp`.
- [Skyrim Souls source](https://github.com/Vermunds/SkyrimSoulsRE/tree/2cbd7d5a0e2a6a04381b4076ac73e8d51332fe32): `src/SkyrimSoulsRE.cpp`, `MenuFlagHandler.cpp`, `SlowMotionHandler.cpp`, and `Controls/MenuControlsEx.cpp`.

Nexus release descriptions and repository HEADs may differ; the hashes pin what was inspected rather than asserting binary equivalence.

## Focused game checks

1. Souls: with Tween unpaused, press Q / Shift+Q in Tab hub, skills and level-up screens. The wheel must stay closed and native skills navigation remain available; after returning to gameplay both entrances must work. Repeat on controller and with Pause / Slow / Normal selected.
2. Important: mark one of two distinct copies Important in Inventory. Favorites mode must show only the ordinary favorite; all-inventory mode must include both without extra counts. Assign a slot to another item, reload, and confirm the Important item remains red-starred/protected and excluded from favorites.
3. STB: assign a numeric key, arbitrary key and chord in Inventory/Magic. Closed-wheel gameplay should fire those bindings. Wheel navigation must not fire STB actions. Check item/spell usage and retained equipment selection, then save/reload. Native number assignment inside the wheel remains unsupported with STB.

## Local validation and delivery

Release build succeeded and deployed to the configured MO2 mod directory. WheelLogicTests verifies unpaused navigation/skills gates and native button forwarding; InventoryTests verifies conditional Important-marker filtering, all-inventory visibility and count partitioning. RuntimeLayoutTests passed for all six supported runtimes and the actual 0.5.3 DLL metadata. Fifteen existing INI/meta files remained byte-identical. The third-party binaries were not installed or run in a game session for this review.

Build and installed DLL SHA256: `300A81B50924823808E02068071EFD2D03E8570830EC1E689301CDF1A015682D`. Local records: `build/compat053-build.txt`, `build/compat053-preserved.json`, and `build/compat053-preservation-check.json`; previous DLL backup: `build/FavoriteWheel-before-compat053.dll`. This remains development version 0.5.3 with the Skyrim skin; the latest formal release remains 0.5.2.

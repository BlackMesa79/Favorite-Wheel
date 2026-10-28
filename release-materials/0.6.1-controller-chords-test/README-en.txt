Favorite Wheel - controller modifier test patch

This test build fixes lost controller modifier state across input callbacks.
It is based on the current 0.6.1 development DLL and also includes the combat
presets feature. It is not a new official release. Diagnostic marker:
state=events-v1 (controllerState=events-v1 in the post-load settings line).

INSTALL
Exit Skyrim. Keep your existing Favorite Wheel installation and install this ZIP
as a separate patch mod in MO2 or Vortex, letting its FavoriteWheel.dll win the
file conflict. Vortex users should deploy it. This ZIP contains only the DLL
and this readme. It does not include or replace your FavoriteWheel.ini.

TEST
1. Use your existing X + Up binding. If configuring it manually, the canonical
   Controls values are GamepadHotkey=266 and GamepadModifier=278. The usual
   GamepadActionModifier=274 is LB; do not hold LB during the favorites test.
2. Launch through SKSE, load a game, hold X first, then press D-pad Up.
3. Release both buttons and repeat with a short and a longer X hold.
4. Release X, then press Up alone. It should not open the favorites wheel when
   X is configured as its required modifier. With modifier -1, Up alone works.
5. Try the separate action-wheel modifier and verify normal paging/release.

REPORT
Check FavoriteWheel.log for state=events-v1. A successful X + Up entrance should
show key=266, favoriteModifier=278, favoriteHeld=true and mode=favorites.
Configured modifier transitions are also logged. If it still fails, send the
Input state after load, Controller modifier and Controller entrance lines.

The old code did not call Papyrus IsKeyPressed, but rebuilt controller state
from a held-duration table each callback. This build retains actual press/release
events across callbacks and clears them on focus/load/device resets. Local
regression tests reproduce the old sparse-packet failure and verify the fix;
the affected setup still needs the test above.

Remove/disable this patch to restore your original DLL. Keep your original mod
and main INI. No settings edits or mod-manager path changes are required.

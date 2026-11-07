Favorite Wheel 0.6.0 - configuration save test patch

This is a test DLL for the configuration-save issue, not a new official release.
It keeps the 0.6.0 plugin version. Diagnostics identify it with writer=file-bytes-v1.

INSTALLATION
Keep Favorite Wheel 0.6.0 installed. Exit Skyrim, then install this ZIP as a
separate patch using MO2 or Vortex and let its FavoriteWheel.dll override the
original DLL. Vortex users should deploy the patch. The ZIP only contains the
DLL and this readme. Back up any custom main INI before testing.

TEST
1. Launch Skyrim through SKSE with your usual mod manager.
2. Open the wheel, press F2, change one setting and select Apply.
3. Exit Skyrim fully and relaunch it. Confirm that setting was retained.
4. If your existing INI is empty or only contains an encoding marker, Apply
   should now replace it with a complete configuration.
5. Check FavoriteWheel.log for writer=file-bytes-v1 and saved-verified with a
   substantial byte count (normally about 1 KB for a fresh UTF-16 document).
   If it fails, share the Settings load/save lines with the stage and Win32 code.

The runtime path remains Data/SKSE/Plugins/FavoriteWheel.ini relative to Skyrim.
MO2 may put a newly created file in overwrite. The original mod package does
not include the player-owned main INI, so its absence in Vortex staging alone
does not indicate a wrong save path. Moving an empty file does not repair it.

The fix directly serializes and validates file bytes instead of relying on the
Windows INI profile API's cached/default reads. It preserves comments, unknown
keys and existing mod-manager mappings. Local tests cover independent-process
reloads, MO2 usvfs and Vortex-style hardlinks; your affected setup still needs
the Apply/restart test above.

To remove the patch, disable this test mod and restore/deploy the original DLL.

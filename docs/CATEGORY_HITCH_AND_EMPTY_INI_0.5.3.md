# Category hitch and empty INI investigation (0.5.3 development)

Follow-up: the reporter's 0.6.0 logs have now arrived and expose a different verification defect. See [CONFIG_SAVE_0.6.0_FOLLOWUP.md](CONFIG_SAVE_0.6.0_FOLLOWUP.md). The findings below describe the earlier investigation.

October 11, 2026. The user reports a short hitch when changing item types and forwards an MO2 user's report that a 0.5.2-generated INI was empty in overwrite and remained empty after moving it into the mod folder. The reporter's log has been requested but is not available. Moving a zero-byte file does not restore its contents.

## Category switching

The local game log contains a 199.55 ms initial font build/upload and three later builds/uploads of 105.80, 108.92 and 89.78 ms while the wheel was open. Normal warmed opening/closing drawing was around 0.2–0.3 ms per frame. These entries identify a real render-thread stall; the existing log does not record the precise category or prove that every reported hitch has this cause.

`ChangeCategory` publishes at most ten cached items without recollecting the inventory. However, `PrepareFonts` previously discovered names only on the visible page. Any new character cleared and rebaked the complete atlas at all native font sizes, then recreated the D3D11 device objects synchronously.

The inventory collection path now gathers a compact union of name/detail/cached-effect characters from the entire catalog before publishing it. Collection runs outside `viewMutex`; the per-frame view carries deduplicated UTF-8 text rather than concatenating every name. Function lists receive the same treatment. The render-thread atlas prewarms these characters together and skips rescanning an unchanged catalog string. Font rebuild logs now include category, function section, actual layout scale and catalog text size.

An offline trace uses five distinct character sets and cycles them three times. Without catalog prewarming, 15 switches caused five font rebuilds, 476.32 ms total font preparation/upload and a 99.31 ms maximum. With prewarming, there were zero switch-time rebuilds, 1.44 ms total and a 0.18 ms maximum; one upfront prewarm cost 94.05 ms. These are a synthetic font-path comparison, not in-game frame-time measurements. The trace remains available with WheelPreview mode `font-categories`.

This removes repeated name-driven rebuilding within a collected directory. Initial opening, a changed font/scale, newly discovered effect descriptions, new inventory items or a newly entered function list can still require font work. It does not solve arbitrary GPU/driver stalls or expensive game-side detail collection. Check real repeated type switches after deployment and correlate any remaining hitch with `Native-size font atlas` and `Inventory detail` entries.

## Empty INI

Startup remains read-only: a missing main INI uses defaults, and an existing empty file stays empty until the player presses Apply. Startup does not autogenerate it. Successful Apply requires a staged settings round trip, destination-byte verification and reopening the target. The reporter's wording alone does not establish whether Apply was pressed, which DLL generated the file, whether a write failed or whether a different configuration source was inspected.

An isolated test uses the locally installed MO2 usvfs 0.5.6.1, a unique VFS instance and scratch directories. It does not connect to the live MO2 instance or alter game/mod/profile configuration. Before the fix, ordinary creation into overwrite and saving mapped empty/nonempty mod INIs all passed, including fresh hooked-process reads. This does not reproduce the reporter's general complaint.

A narrower bug was reproduced: copying an existing zero-byte INI into staging left it without a Unicode BOM. Windows profile writes then used the system ANSI code page; a font path containing a character outside that code page failed `verify-stage`, leaving the original file empty. Missing-file staging already used Unicode. The save path now initializes either kind of empty stage as UTF-16LE. Existing nonempty documents keep their encoding, comments and unknown keys. This is a verified edge-case fix, not a confirmed explanation of the reporter's case.

Load diagnostics now distinguish `loaded-empty bytes=0` from a nonempty load. Verified saves include the byte count. Normal save failures keep their precise stage and Win32 error. The empty-file fix runs only on Apply; it does not silently rewrite player configuration at startup.

## Verification

- SettingsTests: empty-file startup preservation, explicit empty-load diagnostics, successful Unicode repair on Apply, complete reload, existing cancellation/failure/path/independent-process coverage.
- SettingsVfsTests: missing target into overwrite, mapped empty mod file, existing comments/unknown keys, an empty mod INI with a Unicode font path, an empty overwrite INI, and read-only failure without a new shadow file. Successful saves are read by a fresh hooked process. `SettingsVfsTests <path-to-usvfs_x64.dll>` loads the external installed library at runtime; usvfs is not bundled or linked into the mod.
- WheelLogicTests and the category font trace. Existing WheelPreview font/locale/geometry checks remain applicable.

The reporter's log is still needed to identify their actual failure. Relevant lines are `Settings load`, `Settings saved` and `Settings save failed`, including the path, stage, Win32 error and byte count. A `saved-verified` line with nonzero bytes followed by an empty inspected file suggests checking the effective MO2 source and profile; a failure line identifies the operation that needs investigation. Neither outcome should be inferred without the log.

Release build and automatic deployment to the local MO2 FavoriteWheel mod succeeded. The installed DLL matched the build (SHA256 `A38DA1C8ED399DEC074859AC98F53BFB1E6B3D88EE8DB4357AE4E71F2ED63B90`); fifteen installed INI/PNG files remained byte-identical. The previous DLL is backed up as `build/FavoriteWheel-before-category-save053.dll`. No release package or version bump was produced. The new DLL still needs in-game category-switch and Apply/restart verification.

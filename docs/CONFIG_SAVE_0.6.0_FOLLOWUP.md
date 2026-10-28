# Configuration save failure: 0.6.0 follow-up

October 12, 2026. This investigation follows the earlier empty-INI work in
[CATEGORY_HITCH_AND_EMPTY_INI_0.5.3.md](CATEGORY_HITCH_AND_EMPTY_INI_0.5.3.md).
The original 0.6.0 release archives are retained. The reporter has confirmed
successful saving with the test patch; the fix is included in official 0.6.1.

## Evidence from the supplied logs

- `SQxq6wxi.txt`: starts with `load-defaults win32=2` (missing main INI), then three saves fail at `verify-reopened-target win32=13`.
- `YPUqhPxu.txt`: starts with `loaded win32=0 bytes=2`. At 15:01:44 and 15:01:48 it reports `saved-verified win32=0 bytes=2`, then repeated saves fail at `verify-reopened-target win32=13`.
- Both logs are from 0.6.0 and use `D:\SteamLibrary\steamapps\common\Skyrim Special Edition\Data/SKSE/Plugins/FavoriteWheel.ini`.

Two bytes cannot contain the expected INI sections and settings. The old writer
initializes an empty stage with a two-byte UTF-16LE BOM, so a BOM-only document is
consistent with these sizes. The reporter's actual INI bytes were not supplied.
Error 13 is `ERROR_INVALID_DATA`, explicitly emitted by our verification code;
it is not evidence of an access-denied error. The old combined check does not
distinguish a reopened byte mismatch from a settings/profile-cache mismatch.

The old implementation edits a temporary document with
`WritePrivateProfileStringW`, ignores the cache-flush result, then verifies values
through the corresponding profile reads. Those reads can return cached values;
missing keys also return struct defaults. Thus even a completely default-valued
BOM-only document can compare equal to the requested configuration. Byte
verification of the destination only compares it with that defective stage.

Ordinary local and isolated usvfs tests pass with the old writer. A new fault
injection test models profile writes acknowledged and readable from a cache but
never written to disk. The unmodified writer reports exactly
`saved-verified win32=0 bytes=2`; the test rejects that result. This proves the
verification defect and reproduces the key log symptom under a controlled fault.
It does not identify which hook, cache, redirect or environment component caused
the reporter's missing physical stage writes. The Vortex report contains no
failure-stage log, so its exact failure mechanism remains unconfirmed.

## Fix

- Load, edit and serialize the main INI directly from its bytes, without Windows profile APIs or temporary profile files.
- Require every saved field to exist with its explicit serialized value. Default fallbacks cannot satisfy this completeness check.
- Keep comments, unknown sections/keys and line endings; support UTF-8, UTF-8 BOM, UTF-16LE/BE and legacy system-code-page input. Upgrade legacy encoding to UTF-16LE when a new value cannot be represented.
- Update duplicate known keys consistently; reject malformed encoding, oversized files and multiline/NUL values without overwriting an existing document.
- Write through the destination's own handle, flush, verify bytes on that handle, close and reopen to verify the complete file. Keep write-failure rollback. Do not rename over mod-managed destinations.
- Diagnose zero-byte and BOM-only input as `loaded-empty`. Reopen errors retain the actual Windows error; byte mismatches include expected and actual sizes.
- Include `writer=file-bytes-v1` in load/save diagnostics so reporter logs can distinguish this test DLL from the original 0.6.0 writer.
- Startup stays read-only. Apply repairs an empty/BOM-only INI and persists all keys.

## MO2 and Vortex paths

The runtime path remains `Data/SKSE/Plugins/FavoriteWheel.ini`, relative to the
game executable. MO2 resolves it through its virtual filesystem; a new file
normally appears in overwrite, while an existing mapped mod INI is updated in
that mapping. Vortex normally deploys files using hardlinks, so an existing
deployed INI can share its contents with its staging counterpart. A runtime-created
file absent from the original mod package need not automatically appear in staging.

The installation package intentionally omits the player-owned main INI. Its
absence in the staging mod folder is therefore expected before a player manages
that generated file. Changing to a manager-specific staging path would neither
fix defective serialization nor work consistently across managers and profiles.
There is currently no user-facing main-INI save-path setting. Keep the runtime
Data path and fix the writer; do not tell users to move an empty file as a repair.

## Validation

- `SettingsTests`: existing defaults, creation, Unicode, cancellation, clamping, comments/unknown keys, read-only failure and independent-process reload.
- `SettingsPersistenceTests`: active profile-cache fault injection; missing, zero-byte and BOM-only default saves; changed Unicode settings read by a fresh process and the real Windows profile API; Vortex-style hardlinks; UTF-8/BOM, UTF-16LE/BE and ANSI fixtures; duplicate/quoted keys; malformed documents and multiline values preserved on failure.
- `SettingsVfsTests`: isolated local usvfs 0.5.6.1, eight mapping cases including missing/default and BOM/default saves, fresh hooked-process reads and read-only failure.

These tests verify the implementation against controlled filesystem scenarios.
The user reports that the original reporter tested the patch and confirmed
successful configuration saving. This does not establish a separate Vortex
reporter test or confirm a full exit/restart sequence, neither of which was
explicitly included in the feedback. The exact environmental trigger remains
unidentified; the unsafe verification and profile-API dependency are fixed.

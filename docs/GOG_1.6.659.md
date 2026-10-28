# GOG 1.6.659 support (0.6.2)

FavoriteWheel 0.6.2 accepts the exact GOG runtime 1.6.659.0 in the same DLL as the other supported runtimes. Use **GOG SKSE64 2.2.3**, and `versionlib-1-6-659-0.bin` from the matching AE Address Library. Steam SKSE and another runtime's database are not substitutes.

The [official SKSE archive](https://skse.silverlock.org/download/archive/) provides `skse64_2_02_03_gog.7z`. Its file list was checked locally and contains `skse64_1_6_659.dll`. No SKSE files or address databases are redistributed in the FavoriteWheel package.

## Implementation and verification

- Added the exact runtime to `RuntimeSupport::Supported`; unreviewed runtimes and VR remain rejected.
- The pinned CommonLib runtime accessors already cover this version. ControlMap uses the pre-1.6.1130 block at `0xE8`; PlayerCharacter's runtime block uses the post-1.6.629 offset `0x3E0`; ActorState is at `0xC0`. No dependency patches were needed.
- RuntimeLayoutTests now exercise these offsets with independent native-shaped byte fixtures, alongside the existing control/input/player/actor/timer/renderer checks. The 659 database is appended to the optional real-library argument order, preserving prior invocation order.
- Checked the **actual installed** `versionlib-1-6-659-0.bin`: all 21 critical relocation IDs and the PlayerCharacter vtable resolve. These include the input dispatcher, equip/unequip object and spell, poison use, timing and renderer references.
- ActorRuntimeTests now exercise IsDead `0x99`, DrinkPotion `0x10F` and the player update slot `0xAD` for 1.6.659 as well.
- Exported plugin metadata is version 0.6.2; the existing Address Library flags are retained.

These are local structure/dispatch/address tests, not a GOG gameplay test or an inspection of the actual executable's instruction bytes at the input hook. GOG 1.6.659 gameplay verification is pending. Test plugin loading, keyboard/controller opening, ordinary equipment, consumables, combat/outfit presets, time behavior, and save/load on that installation. Keep the matching `.skse` co-save.

Local evidence: `build/release062-RuntimeLayoutTests.log` and `build/release062-ActorRuntimeTests.log`. RuntimeLayoutTests received seven real installed databases for 1.5.97, 1.6.1170, 1.7.99, 1.7.104, 1.6.640, GOG 1.6.1179 and GOG 1.6.659; all passed. The downloaded SKSE archive was inspected only for version confirmation, not installed into the player's game.

# Combat presets (0.7.0)

Version 0.7.0 includes shared native 1–8 combat shortcuts, confirmed in the user’s game tests. See
[binding, conflicts and co-save compatibility](COMBAT_QUICK_SLOTS.md). The
first-stage scope below describes the archived 0.6.2 build.

October 12, 2026. The first stage restores hand equipment from the action wheel.
Initially developed after 0.6.1; included in the official local 0.6.2 package.
The original 0.6.1 archives are retained. See [the release record](RELEASE_0.7.0.md)
for the current DLL hash and verification; hashes below describe earlier builds.

October 13 feedback: the user reports combat presets worked in their testing.
The detailed case/runtime matrix was not supplied, so the extended checks below
remain a reference rather than a claim that every listed combination was tested.

## Use

1. Equip the desired weapons/spells in the actual left and right hands.
2. Open actions (Shift + the wheel opening key by default, or switch wheels with R).
3. Change type with A/D or the configured controller category buttons to Combat.
4. Select **Save hands**, enter a name and confirm.
5. Left-click a preset to restore it; right-click to rename, overwrite from the
   current hands or delete it. Controller use/manage follows the existing A/X
   and trigger/bumper scheme.
6. Save the game to retain new or changed presets. Keep the matching `.skse`
   co-save with that character. Global Apply/INI settings do not save presets.

The detail card lists both hands or one two-handed target. Empty slots are
explicit: a preset can restore an empty left/right hand, or both empty hands.
Selecting an already active preset leaves it equipped; it never toggles it off.
Unavailable presets keep their name and can be managed. Their detail identifies
the missing/changed item, unlearned spell or insufficient matching copies.

This stage includes one-handed weapons and staves, dual weapons/spells, mixed
weapon/spell pairs, shields, carryable torches, bows/crossbows and two-handed
weapons/spells. Two-handed equipment occupies both hands and cannot coexist
with an independent left target. Weapons need not be favorited, and known spells
need not be in Favorites. Spells are equipped for normal casting, never cast
automatically. A bound-weapon spell can be saved as a spell; an inaccessible
conjured weapon itself cannot be captured as an inventory instance.

Apparel, ammunition, voice powers/shouts, scrolls, consumables, automatic poison
use, SkyUI group import and separate group hotkeys are outside this stage.
An existing ammunition selection stays unchanged. Outfit presets remain their
own category with their original format, import/export and apply behavior.

## Identity, execution and persistence

- A physical target records its plugin/local FormID, enchantment, temper value,
  player-set name, and unique ID/owner when present. The label is for display;
  no extra-list pointer is persisted. Matching preserves the original outfit
  signature policy: changes to those identifying attributes make it unavailable.
- Two equivalent non-unique copies can satisfy dual wield, even when represented
  by one count-two stack. One physical copy cannot fill both hands. A unique
  instance cannot be allocated twice, and duplicate unique-ID matches are rejected.
- Apply closes the wheel and releases its pause before submission. The complete
  target is revalidated before clearing equipment. Protected worn quest gear is
  not removed. Equipment operations run on the existing SKSE task path.
- Steps clear incompatible physical gear, equip the right target, equip the left
  target, and verify the whole result. Already-correct hands are skipped. Native
  equip replaces an old spell when the target is nonempty; explicitly empty spell
  slots use the existing Actor.UnequipSpell method through the VM, with its native
  Actor proxy obtained via CommonLib packing. No new ESP or script is shipped.
- Each native call is issued once, then observed rather than repeatedly sent.
  Actual left/right sources are checked independently when clearing a two-handed
  spell. A pending spell callback must complete before advancing. No inventory
  pointer survives a tick or mutation. Polls are throttled to 30 ms, steps have a
  three-second deadline, and the job has a 15-second limit.
- Focus loss, blocked menus, external pause, player invalidation/load or a failed
  operation stops the job. A late engine/mod failure can leave a partial pair;
  the HUD/log reports that outcome. This is not an atomic engine transaction and
  does not promise rollback of arbitrary equipment scripts.
- The existing serialization owner and callbacks are retained. They append/read
  a separate `FWCP` record (version 1, text `FWC 1`) for combat, while keeping the
  old outfit `FWOP` / `FWO 1` record unchanged. Old co-saves initialize combat as
  empty; dynamic FormIDs are remapped through SKSE. Invalid records are rejected.
  Each character can store up to 100 presets, with a bounded 1 MiB record.
- Function navigation exposes Outfits → Combat → optional Face Lighting; the
  follower list stays a child of Lighting. Outfit/combat pages are remembered
  separately. Font prewarming includes all saved outfit/combat names and hand
  labels plus cached lighting names to avoid repeated name-driven type hitches.

## Verification and in-game checks

CombatPresetTests covers physical capacity, worn-hand preference, same-form
enchanted copies, changed signatures, unique-ID collisions, empty and two-handed
schema validation, Unicode serialization, invalid-record rejection and issue/
wait/advance/timeout behavior. It also checks explicit spell clears, pending
callbacks, repeated Apply and the remaining source of a two-handed spell.
WheelLogicTests checks navigation with/without Face Lighting. Existing outfit,
inventory, settings/localization and runtime metadata tests remain applicable.

Production Draw/D3D11 WARP previews use synthetic presets. Modes `combat`,
`combat-lighting`, `combat-empty`, `combat-missing`, `combat-name`, `combat-manage`
and `combat-confirm` cover lists, details and management. The font tests include
saved Unicode names prewarmed across function sections. Offline tests do not
execute native equipment/VM callbacks in a real Skyrim process.

In-game validation still required:

- Distinct enchanted/tempered same-form swords in opposite hands; equivalent
  plain copies; losing one copy should disable dual-wield restoration.
- Same spell in both hands; two different spells; weapon/spell in either hand.
- Sword/shield and sword/torch; bow/two-handed weapon/spell versus two separate
  hand targets. Confirm repeated Apply keeps the pair equipped.
- Empty left/right and both empty, including clearing spells and transitioning
  from a two-handed spell. Confirm no clothing, ammo or voice selection changes.
- Rename/overwrite/delete, multiple pages, Pause/Slow/Normal selection and
  controller A/X/category navigation. Interrupt a job with a menu/focus change.
- Save/fully exit/relaunch/load and switch characters; old outfit presets and
  exports must remain unchanged. Check `Combat preset captured`, `Loaded ...
  combat presets`, `apply started`, `issued step` and `finished` log entries.

The affected installations/animation and equipment-mod combinations need those
real-game checks before treating this as a new official release.

Release build and local MO2 deployment completed. CombatPresetTests,
WheelLogicTests, SettingsTests, OutfitTests, InventoryTests and RuntimeLayoutTests
passed. The installed DLL matches the build: SHA256
`03B2DEFC870F208F91527DB97BF1F4083B0EBE35560A32947CD89823387AA62C`.
Baseline checks preserve the main INI, themes, materials and original language
bytes; each of the eight language files only appends the 28 new combat keys.
Both official 0.6.1 ZIPs remain unchanged. Deployment verification/backup paths
are recorded in `build/combat-deploy-verification.txt`.

Final visual QA checked the English lists/details in Classic Gold, Skyrim and
Skyrim Polygon, the English name field, Chinese management/overwrite confirmation
and 720p English missing-item state. Preview JPEGs are in
`build/hands-{theme}-combat{mode}-{width}.jpg`; render logs confirm active-language
font coverage, no name-switch font rebuild after prewarming, finite geometry and
background dimming. No game was launched for this validation.

The combat icon now uses two complete swords crossing at +/-45 degrees to form
an X, with pointed blades, guards, grips and pommels. The rear blade has a small
occlusion gap at the crossing; Save hands uses the same symbol with a separate
add badge. Updated 1440p/720p WARP previews verify the revised artwork.

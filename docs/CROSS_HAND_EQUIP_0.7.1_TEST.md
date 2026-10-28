# Cross-hand equipment test after 0.7.1 (revision 2)

This development build addresses a wheel action that does nothing when a single
one-handed weapon instance is already worn in the opposite hand. On October 15,
2026 the user confirmed revision 2 worked in game and authorized incorporating
it into the formal 0.7.1 installation/source archives. This document retains the
test history; current release status is recorded in RELEASE_0.7.1.md.
Revision 1 was game-tested on October 15, 2026 and only unequipped the weapon.
Its log showed `action=move submitted=false`, followed by a successful separate
equip click. The next build fixes this regression and adds dual-spell collapse.

## Weapon action

The old path submitted the already-worn instance directly to native EquipObject
with the other hand slot. The new path explicitly unequips the source hand first
when the selected stack contains exactly one copy. It then resolves the instance
again before submitting equip to the target hand. Unequipping can delete or merge
an extra-data list, so its previous pointer must never be reused without checking
the live inventory. Revision 1 incorrectly treated UnequipObject's bool as a
success flag and returned early when a successful unequip preserved its extra
list. The bool indicates extra-list destruction, as documented in the
[SKSE native source](https://github.com/ianpatt/skse64/blob/master/skse64/PapyrusActor.cpp).
Revision 2 ignores that flag for flow control and checks whether the fresh
instance is actually no longer worn in the source hand. If resolution fails,
the operation stops with the existing
changed-item notification instead of substituting an arbitrary weapon variant.

Using the same weapon in its current hand still toggles that hand off. Stacks
with spare copies retain the native equip path for dual wield. Two-handed
equipment retains its existing native slot behavior. The selected hand follows
the existing mouse-hand setting and controller actions.

## Spells

Regular one-handed spells continue to call native EquipSpell for the requested
hand without clearing the other hand when not already dual-equipped. The local
game log confirmed Healing and Flames successfully reached both hands with this
path. When the selected spell already occupies both hands, revision 2 clears
only the opposite hand, retaining the hand requested by the click. This uses
the registered native Actor.UnequipSpell method, through the same VM dispatch
as combat presets. No script/ESP or extra dependency is installed, no spell
slot is edited directly and the wheel does not release its pause for the call.
Dispatch is asynchronous; the user subsequently confirmed revision 2 worked in
their game tests. The callback logs the resulting hand slots
on the game thread as `Spell hand clear completed`; a dispatched request alone
is not reported as verified completion.

## Verification and game checks

The production weapon executor is exercised by HandEquipTests using a fake native
inventory that rejects a direct transfer of a worn sole copy. Both native return
cases are covered: successful unequip returning false with a preserved extra
list and returning true with an invalidated identity. Tests cover both transfer
directions, same-hand toggle, multiple copies, a source that remains worn and
failed re-resolution. The spell executor tests both dual-to-single directions,
additive equip and rejected dispatch. These tests
do not execute Skyrim's native equipment manager. The other 17 suites also pass,
including isolated usvfs persistence and seven real runtime address libraries.

Game check list used for revision 2 (user subsequently confirmed it passed):

- One single-copy favorite weapon: right to left, then left to right in one click.
- An enchanted/tempered/renamed copy: the exact instance survives the transfer.
- Two copies: the other hand can be equipped without removing the first.
- One ordinary spell: equip the other hand and confirm both hands retain it.
- With that spell in L+R, click left to retain only L; restore L+R, then click
  right to retain only R. Do not reverse the requested and cleared hand.
- Repeat with Pause + KeepOpen and with swapped mouse-hand settings.

For remaining spell failures, retain FavoriteWheel.log after the failed click;
the `Spell hand request` line identifies whether a target-hand call was submitted
and what the player slots reported. Weapon requests use `Weapon hand request`.

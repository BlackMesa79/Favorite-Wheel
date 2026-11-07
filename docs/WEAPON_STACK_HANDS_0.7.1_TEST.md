# Plain weapon stacks and hand equipment after 0.7.1

Reported October 16, 2026. The user confirmed both the stack fixes and revision 2
cursor fix in local gameplay and authorized release as 0.7.2. The formal 0.7.1
archives remain unchanged. This document retains the investigation/test history;
see RELEASE_0.7.2.md for the current release status.

## Evidence

The supplied FavoriteWheel (1).log and FavoriteWheel.log share the same startup
and session (runtime 1.5.97); the latter continues later. They are overlapping
evidence, not two independent reproductions. For form 00013989, repeated
opposite-hand requests used `action=equip count=2 submitted=true` with the same
extra-list address. Submitted means the native call was issued, not that a
second hand actually equipped. Other forms (including 0002C66F and 54006168)
alternated between count-two equip requests and count-one move requests, which
stopped with `submitted=false`. The user observed a successful unequip followed
by the changed-item notification. This matches a split/merge identity problem,
not evidence that steel swords need a form-specific rule.

The previous collector added the unextended remainder to one favorite list's
display count. Equip then submitted that same list even if it already represented
the worn copy; a second physical copy was never selected explicitly. After a
native split, the worn copy and favorite marker could live on different lists.
The next action could see only a count-one row and incorrectly move it, or fail
to resolve the old list after unequip. The logs do not include every native list
or its extra-data types, so exact engine allocation details remain inferred.

## Test implementation

- Partition physical counts before filtering. For ordinary weapons, combine
  compatible plain lists and their unextended remainder into one logical row.
  Counts, L/R/L+R state, and equip selection use the same pool model.
- If the requested hand is occupied by a pool member, toggle that actual member.
  Otherwise select an unworn copy. A worn plain list with spare count uses native
  spare selection instead of resubmitting its worn pointer. Only a genuine sole
  copy uses the already-tested move sequence.
- Plain rows can re-resolve a new favorite anchor after native splitting or
  merging. Exact variants still require their current identity. This does not
  mutate inventory stacks, add favorite tags, or copy enchanted-item metadata.
- Only count, worn, worn-left and hotkey extra types qualify as plain. Ownership,
  charge, poison, names, unique IDs, health, enchantment and unknown extra types
  remain separate. Important markers are excluded while their provider is loaded.
  Independently assigned hotkeys remain separate; unbound favorite copies may
  join the bound pool. Updating a shortcut also updates its open-wheel pool marker.
- New `Weapon stack selection` logs identify the logical count, physical lists,
  selected pointer and native-spare path. `Weapon exact selection` reports the
  extra types of protected variants. Hand-request logs also record the resulting
  base forms in each hand; submission alone is not declared native success.

`WeaponStack.h` is shared by production collection/resolution/equip and the
HandEquip tests. Tests cover unextended and explicit unfavorited spares, worn
anchor refusal, both hand toggles, native anchor migration, shared count lists,
single-copy fallback, metadata exclusion, independent bindings and count
conservation. InventoryTests covers shortcut snapshot updates. These run without
Skyrim and do not claim to reproduce the reporter's actual native allocation.

## Game verification

The user confirmed the stack/dual-wield fix worked locally on October 16, 2026.
They then observed cursor recentering only on the first steel-sword equip. The
local log shows that equip changing the favorite anchor from 14982A2C600 to
1498F5304A0 while preserving the same form and total count. PublishDirectory was
still comparing exact ItemKey addresses, so its replacement-item guard reset aim.
Revision 2 compares logical plain-stack selection keys (form, raw binding and
favorite state) for this refresh decision. Exact instances retain their full
key comparison, and an absent/replaced row still resets aim. Hover detail
validation continues to use the current physical instance, not the selection key.
InventoryTests covers anchor migration, quantity changes, different bindings,
different forms, transitions to exact variants and changed exact-instance keys.
The user subsequently confirmed the cursor fix also passed local game testing.
The reporter's original 1.5.97 environment has not separately validated either
test revision yet.

Use two otherwise identical ordinary swords, favorite one and leave the other
unfavorited. Test both split-looking steel swords and a normally stacked pair:
equip right, equip left to obtain L+R, toggle each hand separately, and equip
again without reopening the wheel. Repeat with Pause + KeepOpen and swapped
mouse hands. Also verify one-copy moves and distinct modified copies do not
regress. If it fails, retain the new stack/exact-selection lines and the following
hand-request lines from FavoriteWheel.log.

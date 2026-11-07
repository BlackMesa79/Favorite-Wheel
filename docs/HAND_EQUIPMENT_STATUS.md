# Hand equipment status

Developed after 0.6.1, October 12, 2026; the user accepted the display and layout.
Included in 0.6.2 together with the separate combat-preset feature. Existing
0.6.1 release archives remain unchanged. See [the release record](RELEASE_0.6.2.md).

## Display

Equipped hand-held entries replace their equipped diamond with a small
theme-colored badge: `L` (left), `R` (right), `L+R` (equipped in both hands), or
`2H` (one two-handed weapon/spell). `L` sits to the local left of the inner
quick-slot number and `R` to its local right; `L+R` sits on the outer left and
`2H` on the outer right. Left/right follow each sector's orientation
(counterclockwise/clockwise), not fixed screen directions. Inner quick-slot
numbers are unchanged, including their positions; unbound entries use the same
hand-badge positions without drawing a placeholder number.
Apparel, ammunition, voice powers, outfits and other non-hand actions keep their
existing equipped marker. The detail card and central selection status spell out
the hand state using all eight bundled languages. Sector status lines remain
to quantity only for equipped hand-held items, since the badge already identifies
equipped state. Their long sector names leave room for the inner badges; the
detail card retains the complete name and localized equipment state. Inner badges
use a compact fixed size; label text scales down if a custom font exceeds it.

Physical objects use the exact extra-list instance's `ExtraWorn` and
`ExtraWornLeft`, not a base-form comparison that could incorrectly mark two
different enchanted/tempered copies as the same equipped weapon. Shields and
carryable torches always map a worn instance to the left hand. Two-handed melee
weapons, bows, crossbows and two-handed scrolls use `2H` when worn. Ordinary
spells compare both actual player hand slots; two-handed spells use `2H`, while
the same one-handed spell in both slots uses `L+R`.

Hand state is snapshot data collected on the game thread. Draw never accesses
engine objects. Existing post-action and equip-event directory refreshes update
the labels, including ordinary equipment changes while Keep open/Pause is active.
The instance keys, equip/unequip behavior and persistent data formats are unchanged.

## Validation

- WheelLogicTests covers opposite-hand instances, one spell in both hands,
  two-handed single-slot flags, left-only shield/torch flags and unequipped objects.
- LayoutSkinTests covers 6,480 badge cases across all three supported shapes,
  stretch limits, scales, badge widths and ten sectors. Checks every badge corner
  stays in its own sector and inside the rim, and checks separation from the
  full hover-arrow bounds, inner quick-slot keycap (with extra clearance), icon,
  name and quantity line. Checks also enforce the requested side and inner/outer
  edge for each of the four hand states.
- SettingsTests checks the full eight-language catalogs, including the new keys.
- WheelPreview modes `hands`, `hands-top`, `hands-spell` use production Draw/D3D11
  WARP with synthetic equipment; cover Classic Gold, Skyrim and Skyrim Polygon,
  1440p English and 720p Chinese, and simultaneous hand/quick-slot/hover markers.

In-game checks still required: distinct enchanted copies in opposite hands;
equipping a weapon to the other hand with Keep open enabled; a spell in one/both
hands; bows/two-handed weapons; shields/torches; and no hand badges on apparel,
arrows or shouts/powers. Check both Favorites and All inventory, all three themes,
and Pause/Slow/Normal refreshes. No new release upload is implied by local tests.

Release build and local MO2 deployment completed. InventoryTests, SettingsTests,
WheelLogicTests, LayoutSkinTests and RuntimeLayoutTests passed. Installed DLL
matches the build; the final DLL hash and deployment checks are recorded in
`build/hand-status-layout-verification.txt`.
The player main INI and both official 0.6.1 archives retain their pre-change
hashes. A previous DLL is retained as `build/FavoriteWheel-before-hand-status.dll`.
Final preview files are `build/hands-{classic,skyrim,polygon}-hands-top-2560.jpg`;
Skyrim visual review confirms clear hand labels, inner numbers and hover arrow,
with the localized state in the detail card. These previews use synthetic data.

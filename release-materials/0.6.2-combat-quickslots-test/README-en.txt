Favorite Wheel 0.6.2 development test: combat quick slots and font fallback

This separate test package contains the changes after the archived 0.6.2 release.
Install the full test ZIP with your mod manager. Disable older controller/config
test patches so their DLLs cannot override this build. Preserve your main INI,
custom language/theme files and matching .skse co-saves. The package does not
include a player settings INI. Corresponding source is supplied separately.

COMBAT QUICK SLOTS
Open actions -> Combat, hover an existing preset, and press 1-8 to bind it.
Press the same number again to unbind. The inner sector badge shows the slot.
Close the wheel and press that native shortcut in gameplay to apply the preset.
Save your game after edits; bindings belong to the character's SKSE co-save.

Assigning a combat slot clears its previous native item/spell/shout and combat
owner while keeping items favorited. Assigning an item in the wheel clears its
combat owner only after success. A later SkyUI/vanilla item assignment takes
priority on gameplay press: the native event passes unchanged and the old
combat binding is removed. Unbinding does not restore the displaced item slot.
Overwrite/rename keeps bindings; deleting a preset releases them.

No defaults are bound. Modifiers plus numbers, extended 9/0 slots and menus do
not apply combat presets. Wheel opening keys retain priority. Missing equipment
uses the existing preflight; applying an active preset does not toggle it off.
These shortcuts use native keyboard slots, not independent controller chords.

Original combat equipment records remain FWC1/FWCP version 1. New shortcut data
uses an independent FWQ1/FWCK record, so the original 0.6.2 DLL can still read
equipment presets. Saving after rolling back drops unknown shortcut records.

FONT FIX
Missing/invalid font files now use an installed system font as the primary;
the emergency pixel font is only used when no real font can load. Category
arrows are anti-aliased vector shapes. Valid custom font choices remain honored.
The same pixelated letters/arrows were reproduced and fixed in native 4K WARP
renders. This matches the report but the reporter's actual font remains unknown.
If it persists, send the new FavoriteWheel.log lines beginning 'Wheel font
source' and 'Native-size font atlas' to identify the font and output dimensions.

LOCAL CHECKS
16 local test suites passed, including record migration/order/corruption,
slot transfer/unbind, input ownership and native-event conflict passthrough.
English 1080p and Chinese 720p shortcut layouts passed native visual checks.
Missing fonts at 1080p/4K and a valid custom Georgia font at 4K passed. A prewarmed
4K category-switch run required zero font rebuilds. In-game follow-up is pending
for the new shortcuts and the original pixelation reporter.

Suggested game tests: item -> combat -> another combat -> item on one slot;
same-number unbind; SkyUI item assignment after combat binding; repeated Apply;
rename/overwrite/delete; save, quit and reload; missing items; menus and rapid
presses while an equip job is already running.

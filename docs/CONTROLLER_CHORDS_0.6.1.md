# Controller modifier failure investigation

The fix described here was initially supplied as a 0.6.1 development test patch.
It is included in 0.6.2; disable the separate patch when upgrading. Build hashes
and patch paths below are historical. See [0.6.2](RELEASE_0.6.2.md) for the current
package and verification. The reporter's exact controller environment still
needs its in-game follow-up.

October 13, 2026. The reporter can save settings and open with Up alone, but
X + Up does not open the wheel. Their 4,116-byte log is from a 0.6.0 test DLL
using `writer=file-bytes-v1`, on GOG runtime 1.6.1179.

## Evidence and limits

At 13:52:32–13:56:22, repeated entrance records report `raw=1 key=266 mapped=1
effective=266 mode=none open=false blocked='none'`. Up is reaching the hook with
the expected main key; the eligibility gate does not block it. Given that main
key, the opening policy returns `none` when neither required modifier matches.
The old log does not contain the controller modifier configuration, X events or
device-button table, so it cannot establish which physical X event was received
or prove that the duration table was missing on this particular installation.

The referenced [Reddit report](https://www.reddit.com/r/skyrimmods/comments/r8d6hh/skse_iskeypressed_not_working_for_controllers/)
describes Papyrus IsKeyPressed failing for controllers and suggests retaining
OnKeyDown/OnKeyUp state, with load resets. Favorite Wheel does not call that
function. However, it previously cleared its entire controller state on every
dispatch and rebuilt it from `BSInputDevice::deviceButtons[*].heldDownSecs > 0`,
then applied only the button events present in that batch. CommonLib's local
BSInputDevice::IsPressed uses that same duration-table test. A missing/zero/stale
table plus sparse modifier events loses X between the X and Up packets.

The regression test models exactly that sequence. The old reconstruction returns
`Opening::None`; the event tracker returns `Opening::Favorites`. This proves a
source defect matching the symptom; reporter-side validation remains necessary
to determine whether it explains the whole reported case.

## Fix

- Retain the 16 canonical controller buttons (266–281) from ButtonEvent positive
  values and zero-value releases across callbacks. Do not poll or rebuild from
  held durations. State uses an atomic bit mask because callback thread identity
  can vary in the supplied log.
- Apply all events in a batch before chord evaluation, preserving main-before-
  modifier handling. Quiet polls and the same controller delegate do not clear
  state. Confirmed disconnect/delegate replacement, focus loss and game/load
  resets clear it.
- Preserve the original opening policy, action-wheel priority, native main-key
  down requirement, input ownership, and D-pad/trigger paging edge logic. This
  does not introduce a new controller API or Windows key polling.
- Log configured controller keys after load. Configured modifier transitions
  record key/value/nativeDown/duration. Entrance records include favorite/action
  modifier keys, held booleans and a canonical-key-index mask, with
  `state=events-v1` for identifying this build.

X is canonical key 278; Up is 266. Holding only X and pressing Up should produce
`favoriteModifier=278 favoriteHeld=true` and `mode=favorites`. LB normally remains
the action modifier (274) and keeps priority if it is also held. The old report
did not include those key values; the new diagnostics distinguish a state loss
from a wrong configured key or another plugin removing the X event.

## Verification

WheelLogicTests exercises sparse packets with a zero duration table, quiet polls,
zero-duration release, reversed same-batch order, single Up, action priority,
trigger modifiers, invalid codes, main-as-own-modifier rejection and focus/load/
disconnect/delegate resets. SettingsTests and RuntimeLayoutTests check existing
configuration behavior and the resulting DLL. No image or in-game test is needed
for the input-state model; an affected controller still needs the real X + Up
test and new entrance/modifier logs.

The source/build and local MO2 installation are updated. The original 0.6.1
release ZIPs are retained. A DLL-only controller test patch is supplied separately
with English instructions, including its diagnostic marker.

Build/local-install/patch DLL SHA256:
`AA85AEA376A2E11E3312CA0732C556356273A48161B1AD259D8365ECE61D3418`.
The patch is `dist/FavoriteWheel-0.6.1-controller-chords-test.zip`; its two entries
are the DLL and English readme, and the archived DLL matches the build. Existing
main INI/themes/materials/language contents and official archives are preserved.

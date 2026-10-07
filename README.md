# Favorite Wheel - Radial Actions

![Favorite Wheel - Radial Actions](release-materials/0.3.16/FavoriteWheel-cover-1280.png)

**Your favorites and actions, at your fingertips.**

An independently implemented SKSE plugin for Skyrim Special Edition. Replace the Favorites menu with a categorized radial interface and access a separate wheel for quick actions. The current action modules are outfit presets and optional Face Lighting controls.

Current version: **0.3.15**. The UI uses Dear ImGui and Direct3D 11; SKSE Menu Framework, an ESP, Papyrus scripts, and SWF assets are not required.

## Features

- **Categorized favorites:** weapons, equipment, potions, food, magic, and other items. Ten entries per page, with additional pages as needed.
- **Separate action wheel:** outfit presets and optional Face Lighting controls occupy independent categories.
- **Item information:** available damage, armor, weight, value, spell cost, and effect or enchantment records appear in the hover details card.
- **Outfit management:** capture currently worn equipment, apply or remove an outfit, rename, overwrite, delete, import, and export presets.
- **Optional integrations:** Face Lighting SKSE public API V1 and compatible Skyrim Text Bridge IME input.
- **Built-in settings:** wheel size and X/Y position, background dimming, mouse sensitivity, bindings, sounds, animations, language, and theme.
- **Original UI:** 29 redesigned outline icons, neighboring category titles, diamond page indicators, and staggered fan-style transitions.
- **Resource support:** English and Simplified Chinese; Classic Gold, Frost, Engraved Gold, and Quiet Slate themes; external language and theme files.
- **Session memory:** each wheel remembers its last category during the current game session. Category changes preserve the cursor position.

## Requirements and runtime compatibility

| Skyrim runtime | Status |
| --- | --- |
| SE 1.5.97 | Tested in-game by a user on an earlier test build |
| AE 1.6.1170 | Tested in-game, including the CommonLib dependency update |
| 1.7.99 / 1.7.104 | Supported by this build; not yet tested in-game |

The plugin accepts these four exact Steam runtimes. Other versions, GOG runtimes, and VR are not supported by this build. Support for these two 1.7 releases does not automatically cover future 1.7.x updates.

Required:

- [SKSE64](https://skse.silverlock.org/), matching your game version.
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444), matching your game version. The supported 1.7 runtimes need their version-5 address databases.
- Microsoft Visual C++ 2015–2022 x64 Redistributable.

Keyboard and mouse are supported. Gamepad navigation is not implemented. Testing does not certify every feature on every runtime or every third-party mod combination. The previous 0.3.13 build was confirmed in-game on 1.6.1170; the new automatic language selection still needs in-game verification.

## Installation

Install the packaged ZIP with your mod manager, enable it, and launch through SKSE. The ZIP contains `SKSE/Plugins` at its root; do not add an extra `Data` folder. A GitHub checkout or GitHub's generated source ZIP is for building the plugin, not a ready-to-install mod.

```text
SKSE/Plugins/FavoriteWheel.dll
SKSE/Plugins/FavoriteWheel.ini
SKSE/Plugins/FavoriteWheel/Languages/*.ini
SKSE/Plugins/FavoriteWheel/Themes/*.ini
```

Mark items and spells as favorites in your normal inventory and magic menus, return to gameplay, and press the Favorites key. SkyUI is optional. Disable competing Favorites-menu replacement features in other mods.

The supplied INI uses `Language=auto`: follow the Windows display language, then fall back to English if no translation matches. F2 settings also offer explicit language selection. Existing manual choices are preserved on upgrade; choose System to enable automatic selection. Fonts are loaded from your own system; no Windows fonts are distributed.

## Default controls

| Action | Control |
| --- | --- |
| Open/close favorites | Q, or your game's Favorites binding |
| Open action wheel | Shift + Favorites binding; key and modifier configurable |
| Switch favorites/actions | R while open; configurable |
| Change category | A / D or Left / Right |
| Change page | W / S, Up / Down, or mouse wheel |
| Select an entry | Mouse; return to the center to clear selection |
| Use/equip right hand/execute | Left click or Enter |
| Equip left hand, where supported | Right click |
| Manage an outfit preset | Right click on the preset |
| Close/back | Esc / Tab; a follower sub-list returns to its parent |
| Open settings | F2 while the wheel is open |

Controller controls (Xbox names; equivalent PlayStation buttons use the same positions):

| Action | Controller |
| --- | --- |
| Open favorites | Game's controller Favorites binding (normally D-Pad Up) |
| Open actions | LB + controller Favorites binding |
| Select an entry | Left stick; release to keep the selection |
| Use / right hand | A |
| Left hand / manage preset | X |
| Change category | LB / RB or D-Pad Left / Right |
| Change page | D-Pad Up / Down |
| Switch wheel | Y |
| Back / close | B |
| Settings | Start while open |

The opening button acts as navigation while the controller wheel is open; use B to close. In dialogs/settings, the left stick moves the pointer, A clicks, X resets a setting and B cancels. LB/RB select the appearance/control settings tabs. Preset names still need keyboard/IME input; no virtual keyboard is included.

Opening the wheel pauses gameplay. Item use and actions execute after the menu closes and gameplay resumes. Animations do not delay those actions. Spells and shouts are equipped for normal casting, not cast automatically. Selecting an already equipped weapon in the corresponding hand or an equipped armor item unequips it.

## Outfit presets

Wear the equipment to capture, open the action wheel, and choose **SAVE CURRENT**. Presets include armor, clothing, jewelry, and other non-shield armor items in your inventory; they do not require favorites.

Applying a preset first removes currently worn managed equipment, then equips the saved outfit. If the complete managed outfit already matches, selecting it again removes it. **Weapons, shields, and ammunition are excluded.** No items are created or retrieved from containers or followers.

Right-click a preset to rename, overwrite, delete, or export it. **Save the game after editing presets:** they are stored per character in the SKSE co-save. Loading an older save restores its preset data. Preserve the matching `.skse` file when transferring saves.

Exports use `Data/SKSE/Plugins/FavoriteWheel/Outfits/Exports/*.fwo`. Place files in the sibling `Imports` folder and select **IMPORT OUTFITS**. The destination character must own matching equipment; imports do not copy items. Dynamic forms and player-created dynamic enchantments cannot currently be exported. The format is not compatible with Outfit Wheeler exports.

Missing or ambiguous items, protected quest equipment, and shield-slot conflicts can prevent a change. Improving, enchanting, or renaming equipment may require recapturing the preset. Other mods can alter or block individual equipment steps; incomplete changes are reported rather than forced or rolled back.

See [outfit implementation notes](docs/OUTFITS.md) for identity matching and serialization details.

## Optional integrations

**Face Lighting SKSE** requires a compatible public API V1, rather than an exact release-number match. When no compatible interface is detected, the Face Lighting category is hidden and the remaining wheel functions stay available. A detected interface that is temporarily not ready retains its status diagnostics.

Controls cover the player, the NPC targeted before opening the wheel, the follower group, and individual followers. Saved preferences are distinct from whether a light is currently visible: Face Lighting's own scene rules still apply. See [integration notes](docs/FACELIGHT_WHEEL.md).

**Skyrim Text Bridge** can provide IME input for preset names. Use the shortcut configured in that mod. Ordinary keyboard input and Unicode clipboard paste remain available without it. See [text input notes](docs/TEXT_INPUT.md).

Ultimate Animated Potions NG and Eating Animations and Sounds SE worked in user testing. Consumables use the game's equipment path so those mods can receive their usual triggers. Specific animation and rendering combinations still require testing.

Poison application is not implemented; use poisons through the inventory. Beast-form controls fall back to the original Favorites menu.

## Settings, themes, and translations

Press F2 (controller: Start) inside the wheel. Appearance and controls have separate tabs. Changes preview immediately; **APPLY** saves them and returns to the wheel. Cancel discards unsaved edits. Manual INI changes require a restart. `Enabled=0` restores the original Favorites menu.

The **CONTROLS** tab provides independent favorites/actions keyboard keys and modifiers. Click a key field, hold the desired Shift/Ctrl/Alt modifiers and press the main key, or cycle modifier combinations separately. Right-click / controller X resets the main key. Exact modifier matching prevents one chord from opening both wheels; identical chords prioritize actions and show a warning. `ActionHotkey=-1` follows the effective favorites key, including a custom override. Keyboard settings do not alter controller bindings.

Controller favorites follows the game binding unless overridden; the favorites and actions modifiers are independently configurable. Controller action modifiers supplement the same main controller key. B is reserved for cancelling button capture; Start/Y retain settings/mode-switch functions while open. Choose a modifier different from the main button. Ordinary Q without the required modifier does not open vanilla Favorites when that same game binding is replaced; blocked contexts and `Enabled=0` yield to the game. Other gameplay shortcut events remain intact while the wheel is closed.

See [input bindings and testing](docs/INPUT_CONTROLS.md) for INI codes and regression coverage.

Preserve your INI and customized resources when upgrading. With MO2, also check Overwrite or the configured output mod for files written by the game.

Copy an existing language or theme INI to a new filename, edit its values, and restart to select it. Language filenames use locale codes such as `fr.ini` or `pt-BR.ini`. Missing translations fall back to English. See the [translation guide](docs/LOCALIZATION.md) for matching rules, fonts, and sharing translations. Use a font covering your chosen language. Themes control colors and supported geometric styling, not arbitrary layouts or image replacement. Background blur is not used.

See the [resource guide](docs/RESOURCES.md) and [item information notes](docs/ITEM_INFORMATION.md).

## Building from source

Use Windows, Visual Studio 2022's x64 C++ tools and Windows SDK, xmake 3.x, and PowerShell 7 for packaging. First-time setup needs network access for dependencies.

```powershell
git clone https://github.com/BlackMesa79/Favorite-Wheel.git
cd Favorite-Wheel
.\scripts\bootstrap.ps1
xmake f -m release -y --deploy_dir=""
xmake build FavoriteWheel
```

The output is `build/windows/x64/release/FavoriteWheel.dll`.

**Set `deploy_dir` explicitly when building.** The development default points to the author's local MO2 mod directory. An empty value disables deployment; set a mod root to deploy automatically:

```powershell
xmake f -m release -y --deploy_dir="D:/Modding/MO2/mods/FavoriteWheel"
xmake build FavoriteWheel
```

Automatic deployment preserves existing settings and customized resources, adding missing language keys. It does not modify MO2's `meta.ini`.

Pinned source dependencies:

- CommonLibSSE-NG v11.0.0, commit `94faaed0c60eddd8347767f2d4d29a97c93bde8c`; the historical directory name is `extern/CommonLibVR`.
- Dear ImGui v1.91.9b, commit `f5befd2d29e66809cd1110a152e375a7f1981f06`.

`bootstrap.ps1` downloads the fixed CommonLib snapshot and verifies its archive hash, and clones the pinned ImGui tag. Dependency sources are not committed to this repository. xmake manages spdlog and Microsoft DirectX libraries. See [dependency provenance](docs/CommonLibSSE-NG-REVISION.txt) and [third-party notices](THIRD_PARTY_NOTICES.md).

Tests are separate, non-default targets:

```powershell
$testTargets = @('WheelLogicTests', 'ActorRuntimeTests', 'SettingsTests', 'OutfitTests', 'NameEditorTests', 'FaceLightClientTests', 'RuntimeLayoutTests', 'ItemInfoTests')
foreach ($testTarget in $testTargets) {
    xmake build $testTarget
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $testTarget" }
    xmake run $testTarget
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $testTarget" }
}
```

Runtime/address checks have optional local game-library inputs. The `WheelPreview` target uses the actual drawing code with synthetic data and D3D11 WARP; previews are not game screenshots. Local checks do not replace in-game tests. See the [test plan](docs/TESTING.md) and [CommonLib upgrade review](docs/COMMONLIB_1.7_REVIEW.md).

Package with `pwsh -NoProfile -File scripts/package.ps1`. It produces `dist/FavoriteWheel-0.3.15.zip` and a source ZIP containing pinned dependency sources. The installation ZIP contains only runtime files under `SKSE/` and a root `readme.txt` with the full GPL and third-party notices. Use `-TestPackage` for a `-test.zip` filename or `-SkipSource` to skip the source archive. Build caches, local game files, and private handoff notes are excluded from Git. Technical documents are mainly in Chinese and include clearly labeled historical development notes; current runtime support is stated above and enforced by the code.

## Feedback and release materials

Report issues with your Skyrim/SKSE versions, mod version, reproduction steps, relevant UI or animation integrations, and `FavoriteWheel.log` from `Documents/My Games/Skyrim Special Edition/SKSE/`. Add a crash log for crashes.

The [release-materials directory](release-materials/0.3.14) contains the original editable SVG cover, PNG exports, English Nexus summary, BBCode description, and local HTML preview. The `.cjs` generators use Node.js; PNG export additionally requires `sharp`.

## License and credits

Copyright (C) 2026 BlackMesa79. Favorite Wheel's own code is licensed under **GPL-3.0**. Modification and redistribution are permitted under that license, without warranty. See [LICENSE](LICENSE). Third-party components retain their respective licenses and notices; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [licenses](licenses).

Thanks to the SKSE, CommonLibSSE-NG, Address Library, Dear ImGui, spdlog, and Microsoft DirectX contributors, and to the users testing different game versions.

Grid Inventory and Outfit Wheeler were feature references. No code, icons, or artwork from either mod is included. Developed with AI assistance and refined through hands-on testing.

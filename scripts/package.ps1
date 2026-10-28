#Requires -Version 7.0
param([switch]$SkipSource, [switch]$TestPackage, [ValidatePattern('^[a-z0-9][a-z0-9-]*$')][string]$TestName='test')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$versionMatch = [regex]::Match((Get-Content -LiteralPath (Join-Path $projectRoot 'xmake.lua') -Raw), 'set_version\("([^"]+)"\)')
if (!$versionMatch.Success) { throw 'Project version missing from xmake.lua.' }
$version = $versionMatch.Groups[1].Value
$buildLabel = if($TestPackage){' - DEVELOPMENT TEST: '+$TestName}else{''}
$dll = Join-Path $projectRoot 'build/windows/x64/release/FavoriteWheel.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build the Release DLL before packaging.' }
$dist = Join-Path $projectRoot 'dist'
$stage = Join-Path $projectRoot ('build/package-' + [Guid]::NewGuid().ToString('N'))
$plugins = Join-Path $stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force $dist,$plugins | Out-Null
Copy-Item -LiteralPath $dll -Destination $plugins
# The main settings INI is player-owned. Ship only language/theme INI resources.
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets') -Destination (Join-Path $plugins 'FavoriteWheel') -Recurse
$readme = @"
Favorite Wheel - Radial Actions $version$buildLabel
Copyright (C) 2026 BlackMesa79
SPDX-License-Identifier: GPL-3.0-only
https://github.com/BlackMesa79/Favorite-Wheel

INSTALLATION AND CONTROLS
Install with your mod manager and launch through SKSE64. Requires SKSE64 and
Address Library matching your game version, plus the Microsoft Visual C++
2015-2022 x64 Redistributable. No ESP, scripts, or SKSE Menu Framework required.

Q opens favorites; Shift + Q opens actions; R switches wheels while open.
A/D change categories; W/S or the mouse wheel change pages. Left click uses
an entry; right click equips to the left hand where supported or manages a
preset. F2 -> Keyboard -> Swap mouse hands -> On -> Apply selects LMB left /
RMB right for item equipment. Presets keep LMB apply / RMB manage. F2 opens settings; Esc/Tab closes or returns from a sub-list.
Q follows your game's Favorites binding unless overridden in settings.
If Favorites is rebound, default actions follow that key with Shift. Set
Controls / Hotkey=16 in the active INI and restart to force physical Q.
Settings / Keyboard configures independent keyboard main keys and Shift/Ctrl/Alt
modifiers. Old configuration files retain Q and Shift+Q defaults.
Controller: Favorites binding opens; LB + Favorites opens actions; left stick
selects; A uses; X equips left hand/manages; LB/RB change categories; LT/RT
equips left/right or uses neutral actions. Controller / Category buttons may
swap those pairs. D-Pad
Up/Down changes pages; Y switches wheels; B closes; Start opens settings.
Custom controller Favorites mappings inherit their modifier too. An identical
Favorites/Actions chord opens Favorites; choose a different Actions modifier
when the game's Favorites chord already requires LB. Plain buttons keep their
normal gameplay use when the Favorites modifier is not held.
Hover a favorite and press 1-8 to bind/unbind a native quick slot. Assigned
entries show their number. Close the wheel and use the normal gameplay shortcut.
Save the game after changing bindings. Native slots are 1-8; this does not add
9/0 slots. Other mods' extended shortcuts pass through outside the wheel.
Version 0.8.0 retains the 0.7.3 reporter-confirmed fix for LB+Y opening vanilla
Favorites with custom controller maps. Current native controller state repairs
temporarily missing modifier events while preserving the unmodified button.
Disable separate controller-chord and combat-cycle test patches when installing this release.

MISC STACKS AND VAMPIRE LORD (0.8.0)
Compatible favorited misc stacks, including instruments, display as one entry.
Vampire Lords use a restricted owned-skill wheel; human equipment, presets,
cycles, settings and shortcut rebinding are disabled in this form. Select a
power, close the wheel and use the normal ability key. Werewolves use vanilla.
No additional Vampire Lord skill-selection sound is included.
Reported PureDark/TRP wheel visibility issues remain unresolved.
Remove older misc/Vampire Lord and audio diagnostic test patches when upgrading.

COMBAT CYCLES (0.8.0)
Eight groups store ordered combat presets. With the wheel open, Shift+1-8 adds
the hovered combat preset or captures current hands. Outside the wheel, the
same chord applies the next usable set. Manage groups, member order and custom
Shift/Ctrl/Alt chords in the actions wheel. Save the game to retain changes.
Hover a combat preset to see its cycle group numbers in the wheel centre.
Membership appears only in the centre; sector labels remain uncluttered.
D-Pad paging follows physical press edges. Release the opening key before
paging; a category needs more than ten entries for multiple pages. Paging
diagnostics are recorded in FavoriteWheel.log.
The separate Controller tab configures its main key, modifiers and category
button scheme. LB/RB always cycles settings tabs; A/X remains available. In settings, the selection stick
moves the pointer, A clicks and X resets. Preset names need keyboard/IME input.

The built-in defaults follow your Windows display language. If no
translation matches, it falls back to English. Bundled languages are English,
Simplified Chinese, French, Brazilian Portuguese, Russian,
Japanese, Korean and German. Thanks to Ardios for the French base translation:
https://next.nexusmods.com/profile/Ardios
Current-version additions are maintained by the project.
Windows fonts are loaded locally and are not redistributed. Japanese/Korean
use automatic installed fonts; missing requested glyphs use system fallbacks.
F2 settings offer System mode
and manual choices. Set Language=auto to follow the system or Language=en to
force English in SKSE/Plugins/FavoriteWheel.ini; restart after manual edits.
Keep your existing INI and custom languages/themes when upgrading.
The installation archive deliberately omits FavoriteWheel.ini. With no main
INI, the plugin uses built-in defaults without writing at startup. The first
successful Apply creates it and any missing parent directories. Existing files
are only read at startup; missing keys use defaults without rewriting the file.
Apply saves global INI settings; no game save is needed for configuration.
Version 0.6.1 fixes empty/BOM-only INIs and failed saves by directly serializing
and validating every saved key. Existing comments and custom keys are preserved.
Remove the separate configuration-save test patch when installing this release.
The INI path is anchored to SkyrimSE.exe/Data/SKSE/Plugins/FavoriteWheel.ini.
If an applied value resets on restart, check the load/save path and Windows
error in FavoriteWheel.log, plus your mod manager's winning INI and profile.
KeepOpen defaults on when missing; an explicit KeepOpen=0 remains off.
Existing manual language choices stay unchanged; select System to opt in.
Translation guide: https://github.com/BlackMesa79/Favorite-Wheel/blob/main/docs/LOCALIZATION.md
COMBAT PRESETS (0.7.0)
Open actions -> Combat -> Save hands to capture current left/right equipment.
LMB applies, RMB manages rename/overwrite/delete. Supports dual weapons/spells,
mixed pairs, shields/torches, two-handed equipment and explicit empty hands.
Spells equip without casting. Applying an active preset does not unequip it.
Applying closes the wheel; missing or changed instances report unavailable.
Presets belong to the character's SKSE co-save: save the game after edits.
Combat presets do not capture apparel, ammunition, shouts, scrolls or consumables;
import/export and independent keyboard/controller chord bindings are not currently provided.
Hand-equipped items show L/R beside the inner quick-slot number and L+R/2H at
the outer sector corners. Details show the full localized state.
Single weapons switch hands in one click. Selecting a regular spell already
in the requested hand clears that hand; the other hand is preserved. A
required two-hand spell clears its occupied hand slots together. Powers,
shouts and Vampire Lord spells retain their existing equip behavior.
Controller modifiers retain held press/release state across sparse input packets;
focus loss, loading and controller changes clear it. Single-key bindings still work.
Disable separate combat/controller/configuration/hand-equipment/weapon-stack test patches when upgrading.

SORTING AND WHEEL MEMORY
T cycles available sorting modes; Shift+T enters or exits manual sorting.
Manual sorting: short LMB release moves earlier, RMB moves later. Hold LMB
and drag onto an occupied sector, then release to insert there; scroll while
holding to change pages. RMB/Esc cancels a drag. Empty/centre drops do nothing.
Source dimming, a following item card and a target outline identify the drop;
disable animations for static feedback. Save the game to preserve ordering.
Weapon damage, apparel armor, weight/value and spell magicka cost modes are
available where relevant. Numeric sorting uses detail-panel values; staff
physical damage is unavailable, ammunition uses base damage and concentration
spell costs are per second. Older plugin versions cannot read new sort modes.
F2 -> Gameplay -> Remember last wheel is opt-in. The normal entrance recalls
the last wheel during this session; the dedicated actions shortcut remains
direct. Loading a save resets the remembered wheel, but keeps the option.
Outfits containing apparel with zero ExtraHealth can now be saved without
changing those instances. Recreate previously rejected outfits after updating.

WEAPON STACKS (0.7.2)
Identical plain weapons can dual wield when only one copy is favorited. Native
stack splits/merges no longer cause false changed-item errors or reset the cursor
on first equip. Modified instances and independently bound copies remain distinct.

COMBAT QUICK SLOTS AND FONT FALLBACK (0.7.0)
Hover an existing combat preset and press 1-8 to bind it; press the same number
again to unbind. Close the wheel and press that native shortcut to apply it.
Assigning a combat slot clears its previous native item/spell and combat owner;
assigning an item in the wheel clears the combat owner. A later SkyUI/vanilla
item binding takes priority on gameplay press, with its event passed unchanged.
Bindings are stored separately in the character's SKSE co-save; save the game
after edits. Original combat equipment records remain compatible with 0.6.2.
No hotkey is assigned automatically. Slots 9/0, modifiers plus numbers, menus
and dialogs do not activate combat presets. Wheel entrances retain priority.
Missing equipment reports unavailable; an active preset stays equipped.
Missing/invalid font files now fall back to installed system fonts before the
emergency pixel font. Category arrows are vector shapes. Custom valid fonts
remain selectable. FavoriteWheel.log records requested/resolved font paths.

Outfit edits require a game save. Preserve matching .skse co-saves.
Weapons, shields, and ammunition are excluded from outfit presets.
Applying/removing an outfit always closes the wheel. Shared pieces stay worn;
new pieces equip first (body slot first), then old leftover apparel is removed.
An exact already worn outfit is removed instead. Native model updates and other
equipment scripts can still affect the transition.
The equipped badge checks that every saved outfit instance is worn, including
manual equipment changes; additional apparel does not hide the badge.
Selecting a preset still removes an exact outfit, or replaces the complete
managed outfit when additional apparel is worn.
Outfits allow up to 64 worn pieces. Equivalent spare copies without a unique ID
are accepted only when all saved signature fields match. Unique-ID collisions
remain invalid. Unavailable preset details name the failing piece and reason;
comparison diagnostics are written to FavoriteWheel.log.
Gameplay / Hide empty categories (HideEmptyCategories=1 by default) skips empty
item types; disable it to show all nine. Fresh defaults place the wheel at
X 72%, Y 46% on the right; existing saved positions are retained.
Item source defaults to Favorites only (General / AllInventory=0).
Q -> F2 -> Gameplay -> Item source -> All inventory -> Apply shows carried items.
Spells, shouts and active powers have separate top-level categories and remain
favorites only, including All inventory mode. Passive abilities are excluded;
physical scrolls remain under Other. Native 1-8 assignment requires a favorite.
Books, ingredients and miscellaneous items cannot be directly used.
Pages carry at most ten entries and hover details load on demand; initial
inventory scanning/sorting and new font glyphs can still cost time in large saves.
F2 -> Gameplay -> Time behavior offers Pause (default), Slow time and Normal speed.
SlowTimePercent defaults to 50 (50..100). Low existing values normalize on load;
the wheel respects external slow time and avoids adding slowdown below 50%.
Gameplay / Keep open after equip (General / KeepOpen=1 by default) retains the
wheel for gear, spells, powers, shouts and other actions. Potions, poisons, food and
outfit presets always close.
In Pause mode ordinary equipment changes keep the game paused.
Outfit presets and consumables close before processing. Slow/Normal speed is preserved.
Settings and outfit dialogs pause in every mode; UI animation timing is unchanged.
The world keeps running in live modes. Detected external time changes close the
wheel without overwriting the other source.
In live modes, a direction held before opening remains active until released.
New movement presses inside the wheel only operate its UI. Movement keys still
held when both wheel menus close resume immediately; gameplay remaps are respected.
By default the left stick keeps the opening movement vector until centered;
new aim from rest only selects entries. Optional Controller / Left stick movement
(GamepadMoveWhileOpen=1) enables free left-stick movement in Slow/Normal speed
while the right stick selects. The scheme changes only after successful Apply.
Pause and dialogs block movement; the right stick also controls dialog pointers
when enabled. Center the right stick after closing before camera control resumes. Attacks and opening keys retain release protection.
Wheel buttons do not activate gameplay actions while the wheel is open,
including RB voice-slot powers. Normal controls remain available outside it.
An entrance disabled by native control groups yields to contextual interactions.
Face Lighting and Skyrim Text Bridge are optional, not bundled dependencies.

THEMES AND UPGRADING
Bundled themes: Classic Gold (rebuilt), Skyrim and Skyrim Polygon.
Back up edited themes, then remove frost.ini, engraved.ini, minimal.ini and
any development ellipse.ini from SKSE/Plugins/FavoriteWheel/Themes when
upgrading. Install the new classic.ini. Copying an update ZIP over an old
folder does not remove retired files. Preserve your main INI and custom IDs.
Copy a theme to a unique filename, edit Name/colors/Style/material/layout,
save as UTF-8 and restart; select it under F2 -> Appearance -> Theme -> Apply.
Theme-making guide:
https://github.com/BlackMesa79/Favorite-Wheel/blob/main/docs/THEME_AUTHORING_0.6.0.md
Torches equip in the left hand and toggle off. Poisons have a separate category
and use native weapon checks/confirmation after closing the wheel.
Outfit presets do not capture virtual finger slots from multi-ring mods.
STB Hotkey System bindings are separate; avoid wheel native 1-8 assignment
when STB is installed. Check load/save diagnostics for MO2 INI problems.

RUNTIME SUPPORT
Skyrim 1.5.97 and 1.6.1170 have been tested in-game.
Skyrim GOG 1.6.659 is supported; local structure, actor dispatch and real address
database checks passed. In-game testing on 1.6.659 is pending. Use GOG SKSE64
2.2.3 and versionlib-1-6-659-0.bin. The old GOG SKSE build is in the official
archive: https://skse.silverlock.org/download/archive/
Skyrim GOG 1.6.1179 is supported but has not yet been tested in-game. Use the
GOG SKSE64 2.2.6 build and versionlib-1-6-1179-0.bin from Address Library.
Steam SKSE and the 1170 address database are not substitutes. Optional
integration plugins need their own compatible GOG builds.
Skyrim 1.6.640 is supported. Local runtime layout and
address-library checks passed; in-game testing on 1.6.640 is pending.
Skyrim 1.7.x is supported but has not yet been tested in-game; the exact
supported 1.7 versions are 1.7.99 and 1.7.104. Use matching SKSE64 and Address
Library v5 for those versions. Other runtimes and VR are not supported.

SOURCE AND LICENSES
This program is free software under GNU GPL version 3, without any warranty.
Corresponding source and build instructions:
https://github.com/BlackMesa79/Favorite-Wheel
A matching source ZIP is supplied separately with the release. The repository
main branch may contain later development; use the release's source archive
when rebuilding this version.

Complete GPL text and third-party notices follow in this file. Third-party
components retain their own licenses and are not relicensed by this project.
Paths mentioned in notices refer to the source checkout, not this mod archive.
The installation archive contains only runtime files and this readme.txt.

"@
if($TestPackage -and $TestName -eq 'combat-cycles-test') {
    $readme += @"

COMBAT CYCLES - DEVELOPMENT TEST
Eight groups default to Shift+1 through Shift+8. With the wheel open, a group
shortcut appends the hovered combat preset or captures your current hands.
Outside the wheel, repeat that shortcut to apply the next usable set.
Actions -> Combat Cycles: LMB applies next; RMB manages rename, bindings,
entries and ordering. Shift/Ctrl/Alt combinations are supported. Bare 1-8
keep their normal quick slots. Clear/remove keeps the underlying presets.
Save the game to retain groups, shortcuts and positions in the SKSE cosave.
Groups contain complete two-hand presets; they do not leave a hand unchanged.
This is an experimental build pending in-game testing, not a new release.
"@
}
if($TestPackage -and $TestName -eq 'misc-vampire-test') {
    $readme += @"

MISC STACKS AND VAMPIRE LORD - DEVELOPMENT TEST (2026-10-24)
Overrides the general beast-form behavior described above for Vampire Lords.
Equivalent non-usable misc pickup instances share a display row. Ownership,
native bindings, names and unknown extra metadata remain separated. This
does not merge inventory records. Multi-instance misc rows cannot be rebound
inside the wheel; individual native bindings are preserved.
Vampire Lords use the game's default vampire race and spell list. Only owned,
active spells from that list are displayed, using their authored equip slots.
Both wheel entrances open this restricted skill list. Human equipment,
function/settings switching, combat/cycle shortcuts and quick-slot rebinding
are disabled in this form. Werewolves still yield to vanilla.
Form changes close an open wheel and invalidate queued actions from the old
form; ongoing equipment jobs stop before subsequent steps.
Six automated suites and an offscreen UI preview passed. In-game validation
is still required. This does not change rendering integration or claim to
fix PureDark/TRP compatibility or all transformation crashes.
The main user INI is not included. Install as a separate MO2 test mod above
the existing $version mod; disable this test mod to restore the previous DLL.
"@
}
$licenseSections = @(
    @{ Title='GNU GENERAL PUBLIC LICENSE v3'; File='LICENSE' },
    @{ Title='THIRD-PARTY ATTRIBUTIONS'; File='THIRD_PARTY_NOTICES.md' },
    @{ Title='CommonLibSSE-NG - GPL-3.0-or-later'; File='licenses/CommonLibSSE-NG-GPL-3.0.txt' },
    @{ Title='CommonLibSSE-NG - Modding and Linking Exceptions'; File='licenses/CommonLibSSE-NG-EXCEPTIONS.txt' },
    @{ Title='CommonLibSSE-NG - retained original MIT notice'; File='licenses/LICENSE-MIT.txt' },
    @{ Title='CommonLibSSE-NG - HDE64 / MinHook notice'; File='licenses/LICENSE-hde64.txt' },
    @{ Title='CommonLibVR - retained historical MIT notice'; File='licenses/CommonLibVR-MIT.txt' },
    @{ Title='Dear ImGui - MIT'; File='licenses/Dear-ImGui-MIT.txt' },
    @{ Title='spdlog - MIT'; File='licenses/spdlog-MIT.txt' },
    @{ Title='DirectXMath - MIT'; File='licenses/DirectXMath-MIT.txt' },
    @{ Title='DirectXTK - MIT'; File='licenses/DirectXTK-MIT.txt' }
)
foreach ($section in $licenseSections) {
    $readme += "`n`n" + ('=' * 72) + "`n" + $section.Title + "`n" + ('=' * 72) + "`n`n"
    $readme += Get-Content -LiteralPath (Join-Path $projectRoot $section.File) -Raw
}
Set-Content -LiteralPath (Join-Path $stage 'readme.txt') -Value $readme -Encoding utf8NoBOM
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
$packageSuffix = if ($TestPackage) { '-'+$TestName } else { '' }
$binaryZip = Join-Path $dist "FavoriteWheel-$version$packageSuffix.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $binaryZip -Force

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$binaryArchive = [IO.Compression.ZipFile]::OpenRead($binaryZip)
try {
    $files = @($binaryArchive.Entries | Where-Object { !$_.FullName.EndsWith('/') })
    if (@($files | Where-Object { $_.FullName.Replace('\','/') -ieq 'SKSE/Plugins/FavoriteWheel.ini' }).Count) {
        throw 'Installation archive must not ship the player-owned FavoriteWheel.ini.'
    }
    $nonRuntimeFiles = @($files | Where-Object { !$_.FullName.StartsWith('SKSE/') })
    if ($nonRuntimeFiles.Count -ne 1 -or $nonRuntimeFiles[0].FullName -ne 'readme.txt') {
        throw 'Installation archive must contain only runtime files and readme.txt.'
    }
    foreach ($entry in $files) {
        $stagedFile = Join-Path $stage $entry.FullName
        if (!(Test-Path -LiteralPath $stagedFile -PathType Leaf)) { throw "Unexpected archive entry: $($entry.FullName)" }
        $entryStream = $entry.Open()
        $algorithm = [Security.Cryptography.SHA256]::Create()
        try { $entryHash = [Convert]::ToHexString($algorithm.ComputeHash($entryStream)) }
        finally { $entryStream.Dispose(); $algorithm.Dispose() }
        if ($entryHash -ne (Get-FileHash -LiteralPath $stagedFile -Algorithm SHA256).Hash) {
            throw "Archive contents differ: $($entry.FullName)"
        }
    }
    $stagedFiles = @(Get-ChildItem -LiteralPath $stage -Recurse -File)
    if ($files.Count -ne $stagedFiles.Count) { throw 'Installation archive is missing staged files.' }
    Write-Output "Installation archive verified: $($files.Count) files, runtime files plus readme.txt only."
} finally { $binaryArchive.Dispose() }

if (!$SkipSource) {
    # Construct from a strict list: no repository internals, builds, local caches or game files.
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $sourceZip = Join-Path $dist "FavoriteWheel-$version$packageSuffix-source.zip"
    $stream = [IO.File]::Open($sourceZip,[IO.FileMode]::Create,[IO.FileAccess]::Write)
    $archive = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
    $hashes = [Collections.Generic.List[string]]::new()
    try {
        $selected = [Collections.Generic.List[IO.FileInfo]]::new()
        foreach ($file in @('xmake.lua','.gitignore','.gitattributes','FavoriteWheel.ini','README.md','LICENSE','THIRD_PARTY_NOTICES.md')) {
            $selected.Add((Get-Item -LiteralPath (Join-Path $projectRoot $file)))
        }
        foreach ($tree in @('include','src','tests','scripts','docs','licenses','assets','release-materials','extern/CommonLibVR','extern/imgui')) {
            foreach ($file in Get-ChildItem -LiteralPath (Join-Path $projectRoot $tree) -File -Recurse -Force) {
                $relative = [IO.Path]::GetRelativePath($projectRoot,$file.FullName).Replace('\','/')
                if ($relative -match '(^|/)(\.git|\.xmake|build|\.claude|\.factory)(/|$)') { continue }
                $selected.Add($file)
            }
        }
        foreach ($file in $selected) {
            $relative = [IO.Path]::GetRelativePath($projectRoot,$file.FullName).Replace('\','/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$file.FullName,$relative,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
            $hashes.Add(((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash + '  ' + $relative))
        }
        $entry = $archive.CreateEntry('SOURCE-SHA256SUMS.txt')
        $writer = [IO.StreamWriter]::new($entry.Open())
        try { foreach ($hash in $hashes) { $writer.WriteLine($hash) } } finally { $writer.Dispose() }
    } finally { $archive.Dispose(); $stream.Dispose() }
    Get-Item -LiteralPath $sourceZip | Select-Object FullName,Length
}
Get-Item -LiteralPath $binaryZip | Select-Object FullName,Length
Write-Output "DLL SHA256: $dllHash"

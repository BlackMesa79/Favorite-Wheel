#Requires -Version 7.0
param([switch]$SkipSource, [switch]$TestPackage)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$versionMatch = [regex]::Match((Get-Content -LiteralPath (Join-Path $projectRoot 'xmake.lua') -Raw), 'set_version\("([^"]+)"\)')
if (!$versionMatch.Success) { throw 'Project version missing from xmake.lua.' }
$version = $versionMatch.Groups[1].Value
$dll = Join-Path $projectRoot 'build/windows/x64/release/FavoriteWheel.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build the Release DLL before packaging.' }
$dist = Join-Path $projectRoot 'dist'
$stage = Join-Path $projectRoot ('build/package-' + [Guid]::NewGuid().ToString('N'))
$plugins = Join-Path $stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force $dist,$plugins | Out-Null
Copy-Item -LiteralPath $dll -Destination $plugins
Copy-Item -LiteralPath (Join-Path $projectRoot 'FavoriteWheel.ini') -Destination $plugins
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets') -Destination (Join-Path $plugins 'FavoriteWheel') -Recurse
$readme = @"
Favorite Wheel - Radial Actions $version
Copyright (C) 2026 BlackMesa79
SPDX-License-Identifier: GPL-3.0-only
https://github.com/BlackMesa79/Favorite-Wheel

INSTALLATION AND CONTROLS
Install with your mod manager and launch through SKSE64. Requires SKSE64 and
Address Library matching your game version, plus the Microsoft Visual C++
2015-2022 x64 Redistributable. No ESP, scripts, or SKSE Menu Framework required.

Q opens favorites; Shift + Q opens actions; R switches wheels while open.
A/D change categories; W/S or the mouse wheel change pages. Left click uses
an entry; right click equips to the left hand where supported or manages an
outfit preset. F2 opens settings; Esc/Tab closes or returns from a sub-list.
Q follows your game's Favorites binding unless overridden in settings.
Settings / Controls configures independent keyboard main keys and Shift/Ctrl/Alt
modifiers. Old configuration files retain Q and Shift+Q defaults.
Controller: Favorites binding opens; LB + Favorites opens actions; left stick
selects; A uses; X equips left hand/manages; LB/RB change categories; D-Pad
Up/Down changes pages; Y switches wheels; B closes; Start opens settings.
Hover a favorite and press 1-8 to bind/unbind a native quick slot. Assigned
entries show their number. Close the wheel and use the normal gameplay shortcut.
Save the game after changing bindings. Native slots are 1-8; this does not add
9/0 slots. Other mods' extended shortcuts pass through outside the wheel.
Controller main key and modifiers are configurable. In settings, the left stick
moves the pointer, A clicks and X resets. Preset names need keyboard/IME input.

The supplied configuration follows your Windows display language. If no
translation matches, it falls back to English. F2 settings offer System mode
and manual choices. Set Language=auto to follow the system or Language=en to
force English in SKSE/Plugins/FavoriteWheel.ini; restart after manual edits.
Keep your existing INI and custom languages/themes when upgrading.
Existing manual language choices stay unchanged; select System to opt in.
Translation guide: https://github.com/BlackMesa79/Favorite-Wheel/blob/main/docs/LOCALIZATION.md
Outfit edits require a game save. Preserve matching .skse co-saves.
Weapons, shields, and ammunition are excluded from outfit presets.
Face Lighting and Skyrim Text Bridge are optional, not bundled dependencies.

RUNTIME SUPPORT
Skyrim 1.5.97 and 1.6.1170 have been tested in-game.
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
$packageSuffix = if ($TestPackage) { '-test' } else { '' }
$binaryZip = Join-Path $dist "FavoriteWheel-$version$packageSuffix.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $binaryZip -Force

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$binaryArchive = [IO.Compression.ZipFile]::OpenRead($binaryZip)
try {
    $files = @($binaryArchive.Entries | Where-Object { !$_.FullName.EndsWith('/') })
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
    $sourceZip = Join-Path $dist "FavoriteWheel-$version-source.zip"
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

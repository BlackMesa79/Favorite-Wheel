#Requires -Version 7.0
param([switch]$SkipSource)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = '0.3.13'
$dll = Join-Path $projectRoot 'build/windows/x64/release/FavoriteWheel.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Build the Release DLL before packaging.' }
$dist = Join-Path $projectRoot 'dist'
$stage = Join-Path $projectRoot ('build/package-' + [Guid]::NewGuid().ToString('N'))
$plugins = Join-Path $stage 'SKSE/Plugins'
New-Item -ItemType Directory -Force $dist,$plugins,(Join-Path $stage 'docs'),(Join-Path $stage 'licenses') | Out-Null
Copy-Item -LiteralPath $dll -Destination $plugins
Copy-Item -LiteralPath (Join-Path $projectRoot 'FavoriteWheel.ini') -Destination $plugins
Copy-Item -LiteralPath (Join-Path $projectRoot 'assets') -Destination (Join-Path $plugins 'FavoriteWheel') -Recurse
foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $file) -Destination $stage
}
Copy-Item -Path (Join-Path $projectRoot 'docs/*.md') -Destination (Join-Path $stage 'docs')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/CommonLibSSE-NG-REVISION.txt') -Destination (Join-Path $stage 'docs')
Copy-Item -Path (Join-Path $projectRoot 'licenses/*.txt') -Destination (Join-Path $stage 'licenses')
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt') -Value "$dllHash  SKSE/Plugins/FavoriteWheel.dll" -Encoding ascii
$binaryZip = Join-Path $dist "FavoriteWheel-$version-test.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $binaryZip -Force

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
        foreach ($file in @('xmake.lua','.gitignore','FavoriteWheel.ini','README.md','LICENSE','THIRD_PARTY_NOTICES.md')) {
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

#Requires -Version 7.0
[CmdletBinding()]
param([switch]$Download, [switch]$SelectedOnly)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cacheRoot = Join-Path $repoRoot 'third_party/backends'
$lock = Get-Content -LiteralPath (Join-Path $repoRoot 'cmake/dependencies.lock.json') -Raw | ConvertFrom-Json
New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
foreach ($backend in $lock.backend_archives) {
    if ($SelectedOnly -and $backend.name -like 'ares-*') { continue }
    $archive = Join-Path $repoRoot $backend.archive.path
    if (-not (Test-Path -LiteralPath $archive)) {
        if (-not $Download) { throw "BACKEND_ARCHIVE_MISSING: descarga explícitamente con -Download: $($backend.name)" }
        Invoke-WebRequest -Uri $backend.archive.url -OutFile ($archive + '.tmp')
        if ((Get-FileHash -LiteralPath ($archive + '.tmp') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $backend.archive.sha256) {
            throw "BACKEND_DOWNLOAD_HASH_MISMATCH: $($backend.name)"
        }
        Move-Item -LiteralPath ($archive + '.tmp') -Destination $archive
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $backend.archive.sha256) {
        throw "BACKEND_ARCHIVE_HASH_MISMATCH: $($backend.name)"
    }
    $extraction = Join-Path $cacheRoot $backend.name
    if (-not (Test-Path -LiteralPath (Join-Path $extraction $backend.name) -PathType Container)) {
        Expand-Archive -LiteralPath $archive -DestinationPath $extraction
    }
    # Verify extracted source bytes against the pinned archive, including build files.
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        foreach ($entry in $zip.Entries) {
            if ($entry.FullName.EndsWith('/')) { continue }
            $path = [IO.Path]::GetFullPath((Join-Path $extraction $entry.FullName))
            if (-not $path.StartsWith($extraction + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                throw 'BACKEND_SOURCE_PATH_INVALID'
            }
            $stream = $entry.Open()
            $sha = [Security.Cryptography.SHA256]::Create()
            try { $expected = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
            finally { $stream.Dispose(); $sha.Dispose() }
            if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
                throw "BACKEND_SOURCE_HASH_MISMATCH: $($entry.FullName)"
            }
        }
    }
    finally { $zip.Dispose() }
}
function Get-Source([string]$Name) { Join-Path $cacheRoot "$Name/$Name" }
function Copy-Source([string]$Source, [string]$Destination) {
    $target = [IO.Path]::GetFullPath($Destination)
    if (-not $target.StartsWith($cacheRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'BACKEND_COPY_PATH_INVALID' }
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    foreach ($child in Get-ChildItem -LiteralPath $Source -Recurse -File -Force) {
        $fileTarget = Join-Path $target ([IO.Path]::GetRelativePath($Source, $child.FullName))
        if ((Test-Path -LiteralPath $fileTarget) -and
            (Get-FileHash -LiteralPath $fileTarget -Algorithm SHA256).Hash -eq
            (Get-FileHash -LiteralPath $child.FullName -Algorithm SHA256).Hash) { continue }
        New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($fileTarget)) -Force | Out-Null
        Copy-Item -LiteralPath $child.FullName -Destination $fileTarget -Force
    }
}
$core = Get-Source 'clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d'
Copy-Source (Get-Source 'clown68000-bb198386ae3ee0cd6d246ff917f9941b02b5be6a') (Join-Path $core 'libraries/clown68000')
Copy-Source (Get-Source 'clownz80-aa4cae3127a5893ba7066bcc2d9e9ba8e88e1c3b') (Join-Path $core 'libraries/clownz80')
Copy-Source (Get-Source 'clowncommon-ecee31fc78ee1eed901431d070239c9c6430607c') (Join-Path $core 'libraries/clowncommon')
foreach ($cpu in @('clown68000', 'clownz80')) {
    Copy-Source (Get-Source 'clowncommon-ddeff174c2121b40284883e0e1e3520f39d99227') (Join-Path $core "libraries/$cpu/libraries/clowncommon")
}
Write-Output 'Candidatos fijados preparados y verificados; todavía no seleccionados.'

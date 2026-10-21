[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InputDirectory,
    [Parameter(Mandatory)][string]$OutputFile,
    [Parameter(Mandatory)][ValidatePattern('^[A-Za-z0-9._-]+$')][string]$ArchiveRoot,
    [Parameter(Mandatory)][ValidateRange(315532800, [long]::MaxValue)][long]$SourceDateEpoch
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$inputPath = (Resolve-Path -LiteralPath $InputDirectory).Path
$outputPath = [IO.Path]::GetFullPath($OutputFile)
$inputPrefix = $inputPath.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if ($outputPath.StartsWith($inputPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release output must be outside the installed input prefix.'
}
if (Test-Path -LiteralPath $outputPath) {
    throw "Refusing to replace existing release archive '$outputPath'."
}

$metadataPath = Join-Path $inputPath 'share/tessera/package-metadata.json'
$sourcePath = Join-Path $inputPath 'share/tessera/SOURCE_AVAILABILITY.md'
if (-not (Test-Path -LiteralPath $metadataPath) -or
    -not (Test-Path -LiteralPath $sourcePath)) {
    throw 'Installed package lacks metadata or corresponding-source offer.'
}
$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
if ($metadata.source.revision -notmatch '^[0-9a-f]{40}$' -or
    $metadata.source.clean_at_configure -ne $true) {
    throw 'Release package must come from a clean, identified source commit.'
}
$sourceOffer = Get-Content -LiteralPath $sourcePath -Raw
if (-not $sourceOffer.Contains([string]$metadata.source.revision)) {
    throw 'Corresponding-source offer does not match package metadata.'
}

$files = [Collections.Generic.List[string]]::new()
Get-ChildItem -LiteralPath $inputPath -Recurse -File | ForEach-Object {
    $files.Add([IO.Path]::GetRelativePath($inputPath, $_.FullName).Replace('\', '/'))
}
$files.Sort([StringComparer]::Ordinal)
if ($files.Count -eq 0) { throw 'Installed package is empty.' }
$forbidden = @($files | Where-Object {
    $_ -match '(?i)(^|/)(roms?|packs?|native-fixtures)(/|$)' -or
    $_ -match '(?i)(^|/)(trust\.toml|make_test_pack\.exe)$' -or
    $_ -match '(?i)\.(md|ay|rom)$' -and $_ -notmatch '(?i)(SOURCE_AVAILABILITY|THIRD_PARTY_NOTICES|upstream-license)\.md$'
})
if ($forbidden.Count -ne 0) {
    throw "Forbidden fixture content in package: $($forbidden -join ', ')"
}

$outputDirectory = Split-Path -Parent $outputPath
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$date = [DateTimeOffset]::FromUnixTimeSeconds($SourceDateEpoch).UtcDateTime.Date.AddDays(-1)
$timestamp = [DateTimeOffset]::new([DateTime]::SpecifyKind($date, [DateTimeKind]::Unspecified), [TimeSpan]::Zero)
$minimum = [DateTimeOffset]::new([DateTime]::new(1980, 1, 1), [TimeSpan]::Zero)
if ($timestamp -lt $minimum) { $timestamp = $minimum }

$stream = [IO.File]::Open($outputPath, [IO.FileMode]::CreateNew)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $false)
    try {
        foreach ($relative in $files) {
            $entry = $zip.CreateEntry("$ArchiveRoot/$relative", [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = $timestamp
            $entryStream = $entry.Open()
            try {
                $source = [IO.File]::OpenRead((Join-Path $inputPath $relative))
                try { $source.CopyTo($entryStream) } finally { $source.Dispose() }
            } finally { $entryStream.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }

$digest = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "$digest  $([IO.Path]::GetFileName($outputPath))"

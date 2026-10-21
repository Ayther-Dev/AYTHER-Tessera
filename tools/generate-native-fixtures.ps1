param(
    [Parameter(Mandatory = $true)][string]$TesseraBuild,
    [Parameter(Mandatory = $true)][string]$EnginePackTool,
    [string]$OutputRoot = (Join-Path $TesseraBuild 'native-fixtures')
)

$ErrorActionPreference = 'Stop'
$build = (Resolve-Path -LiteralPath $TesseraBuild).Path
$packTool = (Resolve-Path -LiteralPath $EnginePackTool).Path
$root = [System.IO.Path]::GetFullPath($OutputRoot)
$romTool = Join-Path $build 'tests/tessera_fixture_roms.exe'
$audioTool = Join-Path $build 'tests/tessera_audio_fixture_generator.exe'
foreach ($tool in @($romTool, $audioTool)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Missing built generator: $tool" }
}
New-Item -ItemType Directory -Force -Path $root | Out-Null
$romRoot = Join-Path $root 'roms'
$fx02Root = Join-Path $root 'fx02'
$fx03Root = Join-Path $root 'fx03'
New-Item -ItemType Directory -Force -Path $romRoot,$fx02Root,$fx03Root | Out-Null
& $romTool $romRoot
if ($LASTEXITCODE -ne 0) { throw 'ROM generation failed' }
& $audioTool --generate $fx02Root
if ($LASTEXITCODE -ne 0) { throw 'FX-02 generation failed' }
& $packTool (Join-Path $root 'test_pack.ay') (Join-Path $root 'trust.toml') 'crc32:rc000001' (Join-Path $romRoot 'ayther_synthetic.md')
if ($LASTEXITCODE -ne 0) { throw 'Canonical pack generation failed' }
& python (Join-Path $PSScriptRoot '../tests/fixtures/graphics-replacement/generate.py') `
    --output $fx03Root --pack-tool $packTool --rom (Join-Path $romRoot 'fx03_rom.md')
if ($LASTEXITCODE -ne 0) { throw 'FX-03 generation failed' }
Copy-Item -LiteralPath (Join-Path $romRoot 'fx03_rom.md') -Destination (Join-Path $fx03Root 'fx03_rom.md')

$expected = @{
    (Join-Path $romRoot 'ayther_synthetic.md') = 'c7666ec2a4d05ad3df8ce14648b5abd5bc4b0f9711c2edcaad228e127070c34f'
    (Join-Path $romRoot 'fx03_rom.md') = '92478b869fc61dcd7a8d457b52ac6e08661c27956c774babf4768f32497d1077'
    (Join-Path $fx02Root 'program.md') = '5296c4fa331fd8cb47663fc7ce3b0abef03364221b309a65f2966c6a8642225c'
    (Join-Path $root 'test_pack.ay') = '7178f88fdf77f711e6058a364737a30eb64dd942dfd7720a82318d4841edb485'
    (Join-Path $root 'trust.toml') = 'c548eec970d435842e65909d9358349924a63c33118e5d2900cec2c26e086a7e'
}
foreach ($path in $expected.Keys) {
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected[$path]) { throw "Fixture hash mismatch: $path" }
}
$manifest = Get-Content -LiteralPath (Join-Path $fx03Root 'manifest.json') -Raw | ConvertFrom-Json
foreach ($item in @(
    @{ Path = (Join-Path $romRoot 'fx03_rom.md'); Hash = $manifest.rom_sha256 },
    @{ Path = (Join-Path $fx03Root 'fx03.ay'); Hash = $manifest.pack_sha256 },
    @{ Path = (Join-Path $fx03Root 'trust.toml'); Hash = $manifest.trust_sha256 }
)) {
    $actual = (Get-FileHash -LiteralPath $item.Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $item.Hash) { throw "FX-03 hash mismatch: $($item.Path)" }
}
Write-Output "FIXTURES_VERIFIED=$root"

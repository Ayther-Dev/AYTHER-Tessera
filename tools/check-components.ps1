[CmdletBinding()]
param()
# Comprueba la verificación de componentes de terceros (spec 008, TP-003).
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fail = New-Object System.Collections.Generic.List[string]
$rows = Import-Csv (Join-Path $root 'legal/DEPENDENCIES.csv')
foreach ($c in @(@('dirkwhoffmann/Moira','MIT'), @('floooh/chips','Zlib'), @('aaronsgiles/ymfm','BSD-3-Clause'))) {
    $r = $rows | Where-Object { $_.component -eq $c[0] }
    if (-not $r) { $fail.Add("CSV: falta la fila de $($c[0])"); continue }
    if ($r.license -notmatch [regex]::Escape($c[1])) { $fail.Add("CSV: $($c[0]) debe declarar $($c[1])") }
    if ($r.revision -notmatch '^[0-9a-f]{40}$') { $fail.Add("CSV: $($c[0]) sin revisión de 40 hex") }
    foreach ($lf in ($r.license_files -split ';')) {
        if (-not (Test-Path (Join-Path $root $lf))) { $fail.Add("CSV: falta el archivo de licencia $lf") }
    }
}
foreach ($d in @('moira','z80','ymfm')) {
    $p = Join-Path $root "docs/components/$d.md"
    if (-not (Test-Path $p)) { $fail.Add("MISSING: docs/components/$d.md"); continue }
    $t = Get-Content $p -Raw
    foreach ($k in @('Licencia', 'Procedencia', 'Estado completo', 'Temporización')) {
        if ($t -notmatch $k) { $fail.Add("docs/components/$d.md sin sección $k") }
    }
}
if ((Get-Content (Join-Path $root 'docs/components/ymfm.md') -Raw) -notmatch 'mute') { $fail.Add('ymfm.md sin análisis de mute por canal') }
$n = Join-Path $root 'docs/components/ayther-abi-header.md'
if (-not (Test-Path $n) -or (Get-Content $n -Raw) -notmatch 'Conclusión') { $fail.Add('ayther-abi-header.md sin conclusión') }
$bad = Get-ChildItem (Join-Path $root 'third_party/licenses') -Recurse -File | Where-Object { $_.FullName -notmatch 'clown|ares' -and (Select-String -Path $_.FullName -Pattern 'GNU (Affero )?(Lesser )?General Public License' -Quiet) }
if ($bad) { $fail.Add("COPYLEFT fuera de referencia: $($bad.Name -join ', ')") }
$tp = Join-Path $root 'legal/THIRD_PARTY_NOTICES.md'
foreach ($k in @('Moira','z80.h','ymfm')) { if ((Get-Content $tp -Raw) -notmatch [regex]::Escape($k)) { $fail.Add("THIRD_PARTY_NOTICES sin sección $k") } }
if ($fail.Count) { $fail | ForEach-Object { Write-Error $_ -ErrorAction Continue }; exit 1 }
Write-Output 'check-components: OK'

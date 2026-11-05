[CmdletBinding()]
param()
# Comprueba los documentos de sala limpia (spec 008, RF-7.1, TP-002).
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fail = New-Object System.Collections.Generic.List[string]
function Need([string]$Path, [string[]]$Patterns) {
    $full = Join-Path $root $Path
    if (-not (Test-Path -LiteralPath $full)) { $fail.Add("MISSING: $Path"); return }
    $text = Get-Content -LiteralPath $full -Raw
    foreach ($p in $Patterns) { if ($text -notmatch $p) { $fail.Add("PATTERN: $Path no contiene /$p/") } }
}
Need 'docs/clean-room-policy.md' @('RF-7\.1', 'caja negra', 'ClownMDEmu', 'ares', 'PQ-14', '(?s)(Regla 1.*Regla 2.*Regla 3.*Regla 4)')
Need 'docs/pr-checklist.md' @('sin código de oráculos', 'RF-7\.1')
Need '.github/PULL_REQUEST_TEMPLATE.md' @('\[ \] .*sin código de oráculos')
Need 'docs/clean-room-register.md' @('(?i)instrument', 'No puede escribir')
if ($fail.Count) { $fail | ForEach-Object { Write-Error $_ -ErrorAction Continue }; exit 1 }
Write-Output 'check-policy: OK'

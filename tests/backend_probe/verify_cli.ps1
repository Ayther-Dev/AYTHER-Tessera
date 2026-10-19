#Requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Probe,
    [Parameter(Mandatory)][string]$FixtureDirectory,
    [Parameter(Mandatory)][string]$CheckpointFile,
    [Parameter(Mandatory)][string]$EvidenceDirectory
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Path $EvidenceDirectory -Force | Out-Null
$cases = @(
    @{Name='restored-count-overflow'; Observer='off'; Count='18446744073709551615'; Error='REQUESTED_FRAMES_EXCEED_TRACE'},
    @{Name='negative-count'; Observer='off'; Count='-1'; Error='REQUESTED_FRAMES_EXCEED_TRACE'},
    @{Name='trace-too-short'; Observer='off'; Count='2501'; Error='REQUESTED_FRAMES_EXCEED_TRACE'},
    @{Name='unknown-observer'; Observer='invalid'; Count='1'; Error='OBSERVER_MODE_INVALID'}
)
foreach ($case in $cases) {
    $report = Join-Path $EvidenceDirectory ($case.Name + '.json')
    if (Test-Path -LiteralPath $report) { throw 'REGRESSION_REPORT_ALREADY_EXISTS' }
    $arguments = @($FixtureDirectory,'full-action.json',$report,$case.Observer,'-',$CheckpointFile,$case.Count)
    $output = (& $Probe @arguments 2>&1 | Out-String)
    $code = $LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $EvidenceDirectory ($case.Name + '.log')) -Encoding utf8
    if ($code -ne 1 -or (Test-Path -LiteralPath $report) -or $output -notmatch $case.Error) {
        throw "CLI_NEGATIVE_REGRESSION_FAILED: $($case.Name)"
    }
}
@{tests=$cases.Count;pass=$true;scope='private probe rejects invalid requests without a success report'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $EvidenceDirectory 'summary.json') -Encoding utf8
Write-Output "PROBE_CLI_NEGATIVES_PASSED: $($cases.Count)"

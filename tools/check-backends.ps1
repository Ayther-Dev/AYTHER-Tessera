#Requires -Version 7.2
[CmdletBinding()]
param([switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$probeRoot = Join-Path $repoRoot 'build/windows-dev/tests/backend_probe'
$evidenceRoot = Join-Path $repoRoot 'build/evidence'
$runRoot = Join-Path $evidenceRoot ('backends/gate0-{0}-{1}' -f [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'), [Guid]::NewGuid().ToString('N'))
$summaryPath = Join-Path $evidenceRoot 'backend-gate.json'
$scriptHash = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash.ToLowerInvariant()
$started = [DateTime]::UtcNow
$exitCode = 1
$childInstances = [Collections.Generic.HashSet[string]]::new()
$runs = [Collections.Generic.List[object]]::new()
$traceCache = @{}
$canonicalTraceCache = @{}
$expectedConfigHash = $null
$controlNames = @('up', 'down', 'left', 'right', 'a', 'b', 'c', 'start')
$proofs = [ordered]@{
    pinned_sources_and_codec = $false
    two_headless_candidates = $false
    engine_sdk_link = $false
    own_replay_3000_two_processes = $false
    original_replay_3000_two_processes = $false
    own_three_checkpoint_continuations = $false
    original_three_checkpoint_continuations = $false
    observation_without_interference = $false
    independent_input_and_audio_oracles = $false
    current_build_and_runner_identity = $false
}
$report = [ordered]@{
    schema_version = 2
    scope = 'private Gate 0 feasibility; no product ABI, MVP, Engine/Runtime product integration or FX-03 acceptance'
    started_utc = $started.ToString('o')
    runner_sha256 = $scriptHash
    skip_candidate_build = [bool]$SkipBuild
    build_jobs = 1
    gate0_pass = $false
    selected_backend = $null
    proofs = $proofs
    runs = $runs
    errors = @()
    phase_1_started = $false
    ci_executed = $false
    pull_requests_created = 0
}
function Assert-Gate([bool]$Condition, [string]$Code) {
    if (-not $Condition) { throw $Code }
}
function Assert-True($Value, [string]$Code) {
    Assert-Gate (($Value -is [bool]) -and $Value) $Code
}
function Assert-Integer($Value, [long]$Expected, [string]$Code) {
    $integer = $Value -is [int] -or $Value -is [long] -or $Value -is [uint32] -or $Value -is [uint64]
    Assert-Gate ($integer -and $Value -eq $Expected) $Code
}
function Assert-Hash($Value, [string]$Code) {
    Assert-Gate (($Value -is [string]) -and $Value -cmatch '^[a-f0-9]{64}$') $Code
}
function Get-Hash([string]$Path) {
    Assert-Gate (Test-Path -LiteralPath $Path -PathType Leaf) "REQUIRED_FILE_MISSING: $Path"
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Get-BytesHash([byte[]]$Bytes) {
    return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($Bytes)).ToLowerInvariant()
}
function Get-TextHash([string]$Text) { return Get-BytesHash ([Text.Encoding]::UTF8.GetBytes($Text)) }
function Read-Json([string]$Path) {
    Assert-Gate ((Get-Item -LiteralPath $Path).Length -le 16MB) "REPORT_LIMIT_EXCEEDED: $Path"
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -Depth 100
}
function Assert-FreshBinary([string]$Executable, [string[]]$Sources) {
    $binary = Get-Item -LiteralPath $Executable
    foreach ($source in $Sources) {
        Assert-Gate ($binary.LastWriteTimeUtc -ge (Get-Item -LiteralPath $source).LastWriteTimeUtc) "STALE_BINARY: $Executable frente a $source"
    }
}
function Get-ProcessInstanceKey([int]$ProcessId, [DateTime]$CreatedUtc) {
    return '{0}/{1}' -f $ProcessId, $CreatedUtc.ToUniversalTime().Ticks
}
function Test-ProcessIdentity {
    # Regression for the real Windows PID recycling observed during qualification.
    $first = [DateTime]::Parse('2026-10-19T19:00:00Z').ToUniversalTime()
    $later = $first.AddSeconds(1)
    Assert-Gate ((Get-ProcessInstanceKey 42 $first) -cne (Get-ProcessInstanceKey 42 $later)) 'RECYCLED_PID_CONFUSED_WITH_SAME_PROCESS'
    Assert-Gate ((Get-ProcessInstanceKey 42 $first) -ceq (Get-ProcessInstanceKey 42 $first)) 'PROCESS_IDENTITY_UNSTABLE'
    Assert-Gate ((Get-ProcessInstanceKey 42 $first) -cne (Get-ProcessInstanceKey 43 $first)) 'DIFFERENT_PROCESSES_CONFUSED'
    return @{tests=3;passed=$true;identity='PID plus OS creation time UTC'}
}
function Invoke-Child([string]$Name, [string]$Executable, [string[]]$Arguments) {
    Write-Host ("Gate 0: {0}" -f $Name)
    $beforeHash = Get-Hash $Executable
    $before = [DateTime]::UtcNow
    $settings = [Diagnostics.ProcessStartInfo]::new()
    $settings.FileName = $Executable
    $settings.WorkingDirectory = $repoRoot
    $settings.UseShellExecute = $false
    $settings.CreateNoWindow = $true
    $settings.RedirectStandardOutput = $true
    $settings.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $settings.ArgumentList.Add($argument) }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $settings
    try {
        Assert-Gate ($process.Start()) "PROCESS_START_FAILED: $Name"
        $childId = $process.Id
        # Windows may recycle a PID after exit. Creation time identifies the OS process instance.
        $created = $process.StartTime.ToUniversalTime()
        $instanceId = Get-ProcessInstanceKey $childId $created
        Assert-Gate ($childId -ne $PID -and $childInstances.Add($instanceId)) "PROCESS_INSTANCE_REUSED: $Name"
        $outputTask = $process.StandardOutput.ReadToEndAsync()
        $errorTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(3600000)) {
            $process.Kill($true); $process.WaitForExit()
            throw "PROBE_TIMEOUT: $Name"
        }
        $output = $outputTask.GetAwaiter().GetResult()
        $diagnostic = $errorTask.GetAwaiter().GetResult()
        $output | Set-Content -LiteralPath (Join-Path $runRoot "$Name.stdout.log") -Encoding utf8
        $diagnostic | Set-Content -LiteralPath (Join-Path $runRoot "$Name.stderr.log") -Encoding utf8
        Assert-Gate ($process.ExitCode -eq 0) "PROBE_FAILED: $Name terminó con $($process.ExitCode); consulta sus logs."
        Assert-Gate ((Get-Hash $Executable) -ceq $beforeHash) "BINARY_CHANGED_DURING_RUN: $Name"
        return [pscustomobject]@{
            name = $Name; process_id = $childId; process_instance = $instanceId; created_utc = $created; started_utc = $before
            finished_utc = [DateTime]::UtcNow; executable = $Executable; executable_sha256 = $beforeHash
        }
    }
    finally { $process.Dispose() }
}
function Invoke-Report([string]$Name, [string]$Executable, [string[]]$Arguments, [string]$ReportPath) {
    Assert-Gate (-not (Test-Path -LiteralPath $ReportPath)) "REPORT_ALREADY_EXISTS: $Name"
    $meta = Invoke-Child $Name $Executable $Arguments
    $file = Get-Item -LiteralPath $ReportPath
    Assert-Gate ($file.CreationTimeUtc -ge $meta.started_utc -and $file.LastWriteTimeUtc -ge $meta.started_utc) "STALE_REPORT: $Name"
    $value = Read-Json $ReportPath
    $runs.Add([ordered]@{
        name = $Name; process_id = $meta.process_id; process_instance = $meta.process_instance
        created_utc = $meta.created_utc.ToString('o'); executable_sha256 = $meta.executable_sha256
        report = [IO.Path]::GetRelativePath($repoRoot, $ReportPath); report_sha256 = Get-Hash $ReportPath
        started_utc = $meta.started_utc.ToString('o'); finished_utc = $meta.finished_utc.ToString('o')
    })
    return [pscustomobject]@{ Report = $value; Meta = $meta; Path = $ReportPath }
}
function Get-Trace([string]$Name) {
    if ($traceCache.ContainsKey($Name)) { return $traceCache[$Name] }
    $path = Join-Path $fixtureRoot $Name
    $trace = Read-Json $path
    Assert-Integer $trace.trace_version 1 "TRACE_VERSION_INVALID: $Name"
    $short = $Name.StartsWith('short-')
    $count = if ($short) { 60 } else { 3000 }
    $action = $Name.EndsWith('-action.json')
    Assert-Gate (@($trace.frames).Count -eq $count) "TRACE_COUNT_INVALID: $Name"
    for ($index = 0; $index -lt $count; ++$index) {
        $frame = $trace.frames[$index]
        Assert-Integer $frame.frame ($index + 1) "TRACE_FRAME_INVALID: $Name"
        Assert-Gate (@($frame.ports).Count -eq 1) "TRACE_PORT_COUNT_INVALID: $Name"
        Assert-Integer $frame.ports[0].port 0 "TRACE_PORT_INVALID: $Name"
        $controls = $frame.ports[0].controls
        Assert-Gate (@($controls.PSObject.Properties).Count -eq 8) "TRACE_CONTROL_COUNT_INVALID: $Name"
        $pressed = $action -and $(if ($short) { $index -ge 20 -and $index -le 39 } else { $index -ge 300 -and $index -le 599 })
        foreach ($control in $controlNames) {
            Assert-Gate ($controls.$control -is [bool]) "TRACE_CONTROL_TYPE_INVALID: $Name/$control"
            Assert-Gate ($controls.$control -eq ($control -eq 'b' -and $pressed)) "TRACE_STIMULUS_INVALID: $Name/$control"
        }
    }
    $entry = [pscustomobject]@{ Json = $trace; Hash = Get-Hash $path; Count = $count }
    $traceCache[$Name] = $entry
    return $entry
}
function Write-Field([IO.MemoryStream]$Stream, [uint32]$Tag, [uint32]$Type, [byte[]]$Value) {
    $tagBytes = [BitConverter]::GetBytes($Tag)
    $typeBytes = [BitConverter]::GetBytes($Type)
    $lengthBytes = [BitConverter]::GetBytes([uint64]$Value.Length)
    $Stream.Write($tagBytes, 0, 4); $Stream.Write($typeBytes, 0, 4)
    $Stream.Write($lengthBytes, 0, 8); $Stream.Write($Value, 0, $Value.Length)
}
function Get-CanonicalTraceHash([string]$Name, [string]$RomHash, [string]$ConfigHash) {
    $key = "$Name/$RomHash/$ConfigHash"
    if ($canonicalTraceCache.ContainsKey($key)) { return $canonicalTraceCache[$key] }
    Assert-Gate ([BitConverter]::IsLittleEndian) 'LITTLE_ENDIAN_HOST_REQUIRED'
    $trace = (Get-Trace $Name).Json
    $inputCache = @{}
    $array = [IO.MemoryStream]::new()
    $wire = [IO.MemoryStream]::new()
    try {
        $countBytes = [BitConverter]::GetBytes([uint64]@($trace.frames).Count)
        $array.Write($countBytes, 0, 8)
        foreach ($frame in $trace.frames) {
            $controls = $frame.ports[0].controls
            $inputKey = ($controlNames | ForEach-Object { if ($controls.$_) { '1' } else { '0' } }) -join ''
            if (-not $inputCache.ContainsKey($inputKey)) {
                $input = [IO.MemoryStream]::new()
                try {
                    $n = [BitConverter]::GetBytes([uint64]8); $input.Write($n, 0, 8)
                    for ($index = 0; $index -lt 8; ++$index) {
                        $assignment = [IO.MemoryStream]::new()
                        try {
                            Write-Field $assignment 1 1 ([BitConverter]::GetBytes([uint32]0))
                            Write-Field $assignment 2 1 ([BitConverter]::GetBytes([uint32]$index))
                            Write-Field $assignment 3 2 ([BitConverter]::GetBytes([uint32][bool]$controls.($controlNames[$index])))
                            $entry = $assignment.ToArray(); $n = [BitConverter]::GetBytes([uint64]$entry.Length)
                            $input.Write($n, 0, 8); $input.Write($entry, 0, $entry.Length)
                        }
                        finally { $assignment.Dispose() }
                    }
                    $inputCache[$inputKey] = $input.ToArray()
                }
                finally { $input.Dispose() }
            }
            $record = [IO.MemoryStream]::new()
            try {
                Write-Field $record 1 3 ([BitConverter]::GetBytes([uint64]$frame.frame))
                Write-Field $record 2 8 $inputCache[$inputKey]
                $entry = $record.ToArray(); $n = [BitConverter]::GetBytes([uint64]$entry.Length)
                $array.Write($n, 0, 8); $array.Write($entry, 0, $entry.Length)
            }
            finally { $record.Dispose() }
        }
        $magic = [Text.Encoding]::ASCII.GetBytes('TSTRACE1'); $wire.Write($magic, 0, $magic.Length)
        Write-Field $wire 1 5 ([Convert]::FromHexString($RomHash))
        Write-Field $wire 2 5 ([Convert]::FromHexString($ConfigHash))
        Write-Field $wire 3 3 ([BitConverter]::GetBytes([uint64]@($trace.frames).Count))
        Write-Field $wire 4 8 ($array.ToArray())
        $result = Get-BytesHash ($wire.ToArray())
        $canonicalTraceCache[$key] = $result
        return $result
    }
    finally { $wire.Dispose(); $array.Dispose() }
}
function Get-OracleCount([long]$Begin, [long]$Count, [long]$TraceCount) {
    $windows = if ($TraceCount -eq 60) { @(@(5,15), @(25,35), @(45,55)) } else { @(@(100,200), @(350,550), @(650,850)) }
    $total = 0
    foreach ($window in $windows) {
        $first = [Math]::Max($Begin + 1, $window[0])
        $last = [Math]::Min($Begin + $Count, $window[1])
        if ($last -ge $first) { $total += $last - $first + 1 }
    }
    return $total
}
function Invoke-Qualified([string]$Name, [string]$Trace, [string]$Observer, [long]$Begin = 0,
                          [long]$Count = 3000, [string]$CheckpointDirectory = '-', [string]$RestoreFile = '-',
                          [switch]$Original) {
    $path = Join-Path $runRoot "$Name.json"
    $arguments = @($fixtureRoot, $Trace, $path, $Observer, $CheckpointDirectory, $RestoreFile, $Count.ToString())
    if ($Original) { $arguments += 'original' }
    $run = Invoke-Report $Name $gateExecutable $arguments $path
    $r = $run.Report
    Assert-Integer $r.schema_version 2 "REPORT_SCHEMA_INVALID: $Name"
    Assert-True $r.pass "QUALIFIER_NOT_PASSED: $Name"
    Assert-Gate ($r.backend -ceq 'ClownMDEmu' -and $r.trace -ceq $Trace -and $r.observer -ceq $Observer) "REPORT_CASE_IDENTITY_INVALID: $Name"
    Assert-Integer $r.process_id $run.Meta.process_id "REPORT_PROCESS_ID_INVALID: $Name"
    Assert-Integer $r.begin_frame $Begin "REPORT_BEGIN_FRAME_INVALID: $Name"
    Assert-Integer $r.frames $Count "REPORT_FRAME_COUNT_INVALID: $Name"
    Assert-Integer $r.completed_frame ($Begin + $Count) "REPORT_COMPLETED_FRAME_INVALID: $Name"
    Assert-Gate (@($r.records).Count -eq $Count) "REPORT_RECORD_COUNT_INVALID: $Name"
    foreach ($field in @('build_id','codec_schema','rom_sha256','config_sha256','trace_sha256')) { Assert-Hash $r.$field "REPORT_HASH_INVALID: $Name/$field" }
    Assert-Gate ($r.build_id -ceq $expectedBuildId -and $r.codec_schema -ceq $expectedCodec) "REPORT_BUILD_OR_CODEC_INVALID: $Name"
    $romHash = if ($Original) { $originalRomHash } else { $ownRomHash }
    Assert-Gate ($r.rom_sha256 -ceq $romHash) "REPORT_ROM_IDENTITY_INVALID: $Name"
    if ($null -eq $script:expectedConfigHash) { $script:expectedConfigHash = $r.config_sha256 }
    Assert-Gate ($r.config_sha256 -ceq $expectedConfigHash) "REPORT_CONFIG_IDENTITY_INVALID: $Name"
    Assert-Gate ($r.trace_sha256 -ceq (Get-CanonicalTraceHash $Trace $romHash $r.config_sha256)) "REPORT_TRACE_IDENTITY_INVALID: $Name"
    Assert-True $r.self_tests.pass "SELF_TESTS_FAILED: $Name"
    Assert-Gate ($r.self_tests.checks -gt 0) "SELF_TESTS_NOT_EXECUTED: $Name"
    Assert-Gate ($r.self_tests.media.status -ceq 'passed' -and $r.self_tests.media.checks -gt 0 -and @($r.self_tests.media.cases).Count -ge 11) "MEDIA_TESTS_NOT_EXECUTED: $Name"
    foreach ($case in $r.self_tests.media.cases) { Assert-Gate ($case.checks -gt 0) "MEDIA_CASE_NOT_EXECUTED: $Name" }
    Assert-True $r.oracles_pass "INDEPENDENT_ORACLES_FAILED: $Name"
    $oracleCount = if ($Original) { 0 } else { Get-OracleCount $Begin $Count (Get-Trace $Trace).Count }
    Assert-Integer $r.independent_oracle_checks $oracleCount "INDEPENDENT_ORACLES_NOT_EXECUTED: $Name"
    Assert-Integer $r.audio_oracle_checks $oracleCount "AUDIO_ORACLES_NOT_EXECUTED: $Name"
    Assert-Gate (($r.temporal_complete -is [bool]) -and -not $r.temporal_complete -and
        ($r.capabilities -band 0x800) -eq 0 -and ($r.capabilities -band 0x1000) -ne 0) "TEMPORAL_OR_STATE_CAPABILITY_MISREPRESENTED: $Name"
    Assert-Gate ($r.cd_zero_scalars_generated -gt 0) "CD_GENERATORS_NOT_DRAINED: $Name"
    $captures = if ($Observer -eq 'off') { 0 } else { $Count }
    Assert-Integer $r.captured $captures "CAPTURE_COUNT_INVALID: $Name"
    Assert-Integer $r.fallbacks $captures "FALLBACK_COUNT_INVALID: $Name"
    if ($Observer -eq 'saturated') { Assert-Gate ($r.event_losses -gt 0) "SATURATION_NOT_OBSERVED: $Name" }
    else { Assert-Integer $r.event_losses 0 "UNEXPECTED_EVENT_LOSS: $Name" }
    foreach ($hashName in @('state','video','pcm')) {
        $digests = [Text.StringBuilder]::new()
        for ($index = 0; $index -lt $Count; ++$index) {
            $record = $r.records[$index]
            Assert-Integer $record.frame ($Begin + $index + 1) "RECORD_FRAME_INVALID: $Name"
            Assert-Hash $record.$hashName "RECORD_HASH_INVALID: $Name/$hashName"
            Assert-Gate ($record.pcm_frames -gt 0) "PCM_FRAME_EMPTY: $Name"
            [void]$digests.Append($record.$hashName)
        }
        Assert-Hash $r.aggregate.$hashName "AGGREGATE_HASH_INVALID: $Name/$hashName"
        Assert-Gate ($r.aggregate.$hashName -ceq (Get-TextHash $digests.ToString())) "AGGREGATE_MISMATCH: $Name/$hashName"
    }
    return $run
}
function Compare-Range($Baseline, $Other, [string]$Name) {
    Assert-Gate ($Baseline.Meta.process_instance -cne $Other.Meta.process_instance) "SAME_PROCESS_REPLAY: $Name"
    foreach ($identity in @('build_id','codec_schema','rom_sha256','config_sha256','trace_sha256')) {
        Assert-Gate ($Baseline.Report.$identity -ceq $Other.Report.$identity) "REPLAY_IDENTITY_MISMATCH: $Name/$identity"
    }
    foreach ($record in $Other.Report.records) {
        $offset = [int]($record.frame - $Baseline.Report.begin_frame - 1)
        Assert-Gate ($offset -ge 0 -and $offset -lt @($Baseline.Report.records).Count) "REPLAY_FRAME_OUTSIDE_BASELINE: $Name"
        $expected = $Baseline.Report.records[$offset]
        foreach ($hashName in @('state','video','pcm')) {
            Assert-Gate ($record.$hashName -ceq $expected.$hashName) "REPLAY_DIVERGENCE: $Name/frame=$($record.frame)/$hashName"
        }
        Assert-Gate ($record.pcm_frames -eq $expected.pcm_frames) "PCM_COUNT_DIVERGENCE: $Name/frame=$($record.frame)"
    }
}
function Test-EngineLink {
    Assert-Gate (-not [string]::IsNullOrWhiteSpace($env:AYTHER_ENGINE_PREFIX) -and
        -not [string]::IsNullOrWhiteSpace($env:AYTHER_ENGINE_DEPENDENCY_PREFIX)) 'ENGINE_PREFIX_REQUIRED: define ambos prefijos del SDK instalado.'
    Assert-Gate ((Test-Path -LiteralPath $env:AYTHER_ENGINE_PREFIX -PathType Container) -and
        (Test-Path -LiteralPath $env:AYTHER_ENGINE_DEPENDENCY_PREFIX -PathType Container)) 'ENGINE_PACKAGE_REQUIRED'
    $engineBuild = Join-Path $repoRoot 'build/windows-engine-integration'
    $cacheText = Get-Content -LiteralPath (Join-Path $engineBuild 'CMakeCache.txt') -Raw
    foreach ($pair in @(@('TESSERA_ENGINE_PREFIX', $env:AYTHER_ENGINE_PREFIX),
                         @('TESSERA_ENGINE_DEPENDENCY_PREFIX', $env:AYTHER_ENGINE_DEPENDENCY_PREFIX))) {
        $match = [regex]::Match($cacheText, ('(?m)^{0}:PATH=(.+)\r?$' -f $pair[0]))
        Assert-Gate ($match.Success -and [IO.Path]::GetFullPath($match.Groups[1].Value.Trim()) -eq [IO.Path]::GetFullPath($pair[1])) 'ENGINE_CACHE_PREFIX_MISMATCH: reconstruye el preset de integración.'
    }
    $engineFiles = @('include/ayther/ayther_sdk_version.h','include/ayther/ayther_version.h','lib/ayther_engine.lib','lib/ayther_core.lib')
    $engineInputs = @($engineFiles | ForEach-Object { Join-Path $env:AYTHER_ENGINE_PREFIX $_ })
    $linkExecutable = Join-Path $engineBuild 'tests/bootstrap/tessera_engine_link_test.exe'
    Assert-FreshBinary $linkExecutable ($engineInputs + @(
        (Join-Path $repoRoot 'tests/bootstrap/engine_link_test.cpp'), (Join-Path $repoRoot 'tests/bootstrap/engine_pod.c')))
    $before = [DateTime]::UtcNow
    & (Join-Path $PSScriptRoot 'check.ps1') -Preset windows-engine-integration -Stage Test -Jobs 1 | Out-Host
    Assert-Gate ($LASTEXITCODE -eq 0) 'ENGINE_CTEST_FAILED'
    $logPath = Join-Path $engineBuild 'Testing/Temporary/LastTest.log'
    Assert-Gate ((Get-Item -LiteralPath $logPath).LastWriteTimeUtc -ge $before) 'ENGINE_TEST_LOG_STALE'
    $log = Get-Content -LiteralPath $logPath -Raw
    Assert-Gate ([regex]::Matches($log, '(?m)^Test Passed\.\r?$').Count -eq 3 -and
        [regex]::Matches($log, '(?m)^\d+/3 Testing: bootstrap\.(dependencies|c11|engine_link)\r?$').Count -eq 3 -and
        $log -notmatch 'Test Failed\.') 'ENGINE_THREE_TESTS_NOT_PASSED'
    $versions = [regex]::Match($log, 'ENGINE_LINK_CHECK headers=(\S+) linked=(\S+) engine=(\S+) core_abi=(\d+)')
    Assert-Gate ($versions.Success -and $versions.Groups[1].Value -ceq $versions.Groups[2].Value -and
        $versions.Groups[1].Value -ceq $versions.Groups[3].Value -and [int]$versions.Groups[4].Value -gt 0) 'ENGINE_VERSION_OR_ABI_MISMATCH'
    Copy-Item -LiteralPath $logPath -Destination (Join-Path $runRoot 'engine-LastTest.log')
    return [ordered]@{
        passed = $true; tests = 3; version = $versions.Groups[1].Value; core_abi = [int]$versions.Groups[4].Value
        test_log_sha256 = Get-Hash $logPath; executable_sha256 = Get-Hash $linkExecutable
        package_files = @($engineInputs | ForEach-Object { [ordered]@{ path = $_; sha256 = Get-Hash $_ } })
        scope = 'installed SDK out-of-line symbol and C/C++ POD link only; product direction tested in T015-T018'
    }
}
Push-Location $repoRoot
try {
    New-Item -ItemType Directory -Path $runRoot -Force | Out-Null
    $report.process_identity_regression = Test-ProcessIdentity
    # No download flag: validate pinned archives, extracted sources and nested copies.
    & (Join-Path $PSScriptRoot 'bootstrap-backends.ps1')
    & (Join-Path $PSScriptRoot 'generate-clown-codec.ps1') -Check
    $proofs.pinned_sources_and_codec = $true
    if (-not $SkipBuild) {
        $cache = Join-Path $repoRoot 'third_party/backends'
        $clown = 'clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d'
        $ares = 'ares-0aafd85789215e84e1e43415c07d4c88461b7899'
        & (Join-Path $PSScriptRoot 'check.ps1') -Preset windows-dev -Jobs 1 -CMakeArgs @(
            '-DTESSERA_BUILD_BACKEND_PROBES=ON','-DTESSERA_PROBE_CLOWN=ON','-DTESSERA_PROBE_ARES=ON',
            "-DTESSERA_CLOWN_SOURCE=$cache/$clown/$clown", "-DTESSERA_ARES_SOURCE=$cache/$ares/$ares")
        Assert-Gate ($LASTEXITCODE -eq 0) 'CANDIDATE_BUILD_OR_CTEST_FAILED'
    }
    $gateExecutable = Join-Path $probeRoot 'tessera_clown_gate_probe.exe'
    $header = Join-Path $probeRoot 'gate_build_id.hpp'
    $match = [regex]::Match((Get-Content -LiteralPath $header -Raw), '#define TESSERA_GATE_BUILD_ID "([a-f0-9]{64})"')
    Assert-Gate $match.Success 'GATE_BUILD_ID_MISSING'
    $expectedBuildId = $match.Groups[1].Value
    $match = [regex]::Match((Get-Content -LiteralPath (Join-Path $repoRoot 'tests/backend_probe/clown_state_codec.cpp') -Raw), 'constexpr std::string_view schema = "([a-f0-9]{64})"')
    Assert-Gate $match.Success 'CODEC_SCHEMA_IDENTITY_MISSING'
    $expectedCodec = $match.Groups[1].Value
    $gateSources = @('clown_gate.cpp','clown_media.cpp','clown_media.hpp','clown_media_tests.cpp',
        'clown_state_codec.cpp','clown_state_codec.hpp','CMakeLists.txt') | ForEach-Object { Join-Path $repoRoot "tests/backend_probe/$_" }
    Assert-FreshBinary $gateExecutable (@($gateSources) + @($header,
        (Join-Path $repoRoot 'cmake/dependencies.lock.json'), (Join-Path $PSScriptRoot 'generate-clown-codec.audit.json')))
    $gateBinaryHash = Get-Hash $gateExecutable
    $report.build_id = $expectedBuildId; $report.codec_schema = $expectedCodec
    $report.qualifier_executable_sha256 = $gateBinaryHash
    # The installed Engine prerequisite is executed afresh, including in SkipBuild mode.
    $report.engine_link = Test-EngineLink
    $proofs.engine_sdk_link = $true
    $fixtureRoot = Join-Path $runRoot 'fixtures'
    $generator = Join-Path $probeRoot 'tessera_backend_probe.exe'
    Assert-FreshBinary $generator @((Join-Path $repoRoot 'tests/backend_probe/main.cpp'))
    $generatorRun = Invoke-Child 'fixture-generation' $generator @('--generate', $fixtureRoot)
    $manifestPath = Join-Path $fixtureRoot 'manifest.json'
    $manifest = Read-Json $manifestPath
    Assert-Integer $manifest.manifest_version 1 'FIXTURE_MANIFEST_VERSION_INVALID'
    Assert-Gate ($manifest.fixture_id -ceq 'tessera-phase0-input-psg-v1' -and @($manifest.artifacts).Count -eq 6) 'FIXTURE_MANIFEST_INVALID'
    $seen = [Collections.Generic.HashSet[string]]::new()
    foreach ($artifact in $manifest.artifacts) {
        Assert-Gate ($artifact.path -in @('program.md','short-control.json','short-action.json','full-control.json','full-action.json','expected.json') -and $seen.Add($artifact.path)) 'FIXTURE_ARTIFACT_INVALID'
        Assert-Hash $artifact.sha256 'FIXTURE_HASH_INVALID'
        $path = Join-Path $fixtureRoot $artifact.path
        Assert-Gate ((Get-Hash $path) -ceq $artifact.sha256 -and (Get-Item -LiteralPath $path).Length -eq $artifact.size_bytes) 'FIXTURE_ARTIFACT_MISMATCH'
    }
    foreach ($trace in @('short-control.json','short-action.json','full-control.json','full-action.json')) { [void](Get-Trace $trace) }
    $ownRomHash = Get-Hash (Join-Path $fixtureRoot 'program.md')
    $originalRomPath = Join-Path $repoRoot 'build/windows-dev/fixtures/ayther_synthetic.md'
    $originalRomHash = Get-Hash $originalRomPath
    $report.fixtures = [ordered]@{
        manifest_sha256 = Get-Hash $manifestPath; generator_sha256 = $generatorRun.executable_sha256
        own_rom_sha256 = $ownRomHash; original_rom_sha256 = $originalRomHash
        traces = @($traceCache.GetEnumerator() | Sort-Object Key | ForEach-Object { @{name=$_.Key; file_sha256=$_.Value.Hash} })
    }
    $partials = @()
    foreach ($candidate in @('clown','ares')) {
        $executable = Join-Path $probeRoot ('tessera_{0}_probe.exe' -f $candidate)
        $sources = if ($candidate -eq 'clown') { @('clown_probe.cpp') } else { @('ares_probe.cpp','ares_runtime.cpp','ares.cmake') }
        Assert-FreshBinary $executable @($sources | ForEach-Object { Join-Path $repoRoot "tests/backend_probe/$_" })
        $comparisons = @()
        foreach ($kind in @('control','action')) {
            $pair = @()
            foreach ($number in 1..2) {
                $name = "partial-$candidate-$kind-$number"; $path = Join-Path $runRoot "$name.json"
                $run = Invoke-Report $name $executable @($fixtureRoot,"short-$kind.json",$path) $path
                $r = $run.Report
                Assert-Gate ($r.trace -ceq "short-$kind.json") "PARTIAL_TRACE_INVALID: $name"
                if ($candidate -eq 'clown') {
                    Assert-Gate ($r.backend -ceq 'ClownMDEmu' -and $r.revision -ceq '88ef45a6585556e2247dd26b6c937d6b9fb4a12d') 'PARTIAL_CLOWN_REVISION_INVALID'
                    Assert-Integer $r.completed_frames 60 "PARTIAL_FRAME_COUNT_INVALID: $name"
                    Assert-True $r.oracle_pass "PARTIAL_ORACLE_FAILED: $name"
                    Assert-True $r.audio_pass "PARTIAL_AUDIO_FAILED: $name"
                    Assert-True $r.copy_readers_pass "PARTIAL_READERS_FAILED: $name"
                    $hashes = $r.aggregate; $names = @('observable','video','psg','fm')
                }
                else {
                    Assert-Gate ($r.backend_commit -ceq '0aafd85789215e84e1e43415c07d4c88461b7899' -and $r.status -ceq 'passed_partial_probe') 'PARTIAL_ARES_REVISION_OR_STATUS_INVALID'
                    Assert-Integer $r.frames_completed 60 "PARTIAL_FRAME_COUNT_INVALID: $name"
                    Assert-True $r.oracles_passed "PARTIAL_ORACLE_FAILED: $name"
                    Assert-Gate ($r.rom_sha256 -ceq $ownRomHash -and $r.trace_sha256 -ceq (Get-Trace "short-$kind.json").Hash) 'PARTIAL_ARES_CONTENT_IDENTITY_INVALID'
                    $hashes = $r.hashes; $names = @('observable_state_sha256','video_sha256','audio_sha256')
                }
                foreach ($hashName in $names) { Assert-Hash $hashes.$hashName "PARTIAL_HASH_INVALID: $name/$hashName" }
                $pair += [pscustomobject]@{ Run=$run; Hashes=$hashes }
            }
            Assert-Gate ($pair[0].Run.Meta.process_instance -cne $pair[1].Run.Meta.process_instance) 'PARTIAL_PROCESS_NOT_INDEPENDENT'
            foreach ($hashName in $names) { Assert-Gate ($pair[0].Hashes.$hashName -ceq $pair[1].Hashes.$hashName) "PARTIAL_REPLAY_DIVERGENCE: $candidate/$kind/$hashName" }
            $comparisons += @{trace="short-$kind.json";frames=60;independent_processes=2;partial_hashes_match=$true}
        }
        $partials += @{name=$candidate;comparisons=$comparisons;acceptance_scope='partial comparison only'}
    }
    $report.partial_candidate_comparison = $partials
    $proofs.two_headless_candidates = $true
    $ownBases = @{}
    foreach ($kind in @('control','action')) {
        $checkpointDirectory = if ($kind -eq 'action') { Join-Path $runRoot 'checkpoints-own' } else { '-' }
        if ($checkpointDirectory -ne '-') { New-Item -ItemType Directory -Path $checkpointDirectory | Out-Null }
        $base = Invoke-Qualified "own-$kind-process1" "full-$kind.json" full -CheckpointDirectory $checkpointDirectory
        $again = Invoke-Qualified "own-$kind-process2" "full-$kind.json" full
        Compare-Range $base $again "own-$kind-3000"
        $ownBases[$kind] = $base
    }
    $proofs.own_replay_3000_two_processes = $true
    foreach ($hashName in @('state','video','pcm')) {
        Assert-Gate ($ownBases.action.Report.records[449].$hashName -cne $ownBases.control.Report.records[449].$hashName) "ACTION_HAS_NO_EFFECT: $hashName/frame450"
    }
    $proofs.independent_input_and_audio_oracles = $true
    $originalCheckpoints = Join-Path $runRoot 'checkpoints-original'
    New-Item -ItemType Directory -Path $originalCheckpoints | Out-Null
    $originalBase = Invoke-Qualified 'original-control-process1' full-control.json full -CheckpointDirectory $originalCheckpoints -Original
    $originalAgain = Invoke-Qualified 'original-control-process2' full-control.json full -Original
    Compare-Range $originalBase $originalAgain 'original-control-3000'
    $proofs.original_replay_3000_two_processes = $true
    $continuations = @()
    foreach ($original in @($false,$true)) {
        $label = if ($original) { 'original' } else { 'own' }
        $directory = if ($original) { $originalCheckpoints } else { Join-Path $runRoot 'checkpoints-own' }
        $baseline = if ($original) { $originalBase } else { $ownBases.action }
        $trace = if ($original) { 'full-control.json' } else { 'full-action.json' }
        foreach ($checkpoint in @(500,1500,2500)) {
            $file = Join-Path $directory "$checkpoint.tsc"
            Assert-Gate ((Get-Item -LiteralPath $file).LastWriteTimeUtc -ge $baseline.Meta.started_utc) "STALE_CHECKPOINT: $label/$checkpoint"
            $restored = Invoke-Qualified "$label-restore-$checkpoint" $trace full -Begin $checkpoint -Count 500 -RestoreFile $file -Original:$original
            Compare-Range $baseline $restored "$label-checkpoint-$checkpoint"
            $continuations += @{
                content=$label;checkpoint_frame=$checkpoint;first_compared_frame=$checkpoint+1;frames=500
                checkpoint_sha256=Get-Hash $file;three_hashes_match=$true;new_process_id=$restored.Meta.process_id
                new_process_instance=$restored.Meta.process_instance
            }
        }
        if ($original) { $proofs.original_three_checkpoint_continuations = $true }
        else { $proofs.own_three_checkpoint_continuations = $true }
    }
    $report.checkpoint_continuations = $continuations
    $off = Invoke-Qualified 'observation-off' short-action.json off -Count 60
    foreach ($observer in @('full','saturated')) {
        $observed = Invoke-Qualified "observation-$observer" short-action.json $observer -Count 60
        Compare-Range $off $observed "observation-$observer"
    }
    $proofs.observation_without_interference = $true
    Assert-Gate ((Get-Hash $gateExecutable) -ceq $gateBinaryHash -and (Get-Hash $PSCommandPath) -ceq $scriptHash) 'BUILD_OR_RUNNER_CHANGED_DURING_GATE'
    $proofs.current_build_and_runner_identity = $true
    $report.gate0_pass = @($proofs.Values | Where-Object { $_ -ne $true }).Count -eq 0
    Assert-True $report.gate0_pass 'GATE0_PROOF_INCOMPLETE'
    $report.selected_backend = 'ClownMDEmu'
    $report.rejected_candidates = @(@{name='ares';selected=$false;reason=@(
        'serialize(false) persists coroutine stacks/pointers',
        'synchronized serializer neutrality not established',
        'controller latches and Stream DSP/filter/resampler/pending samples remain outside a complete explicit codec')})
    $report.remaining_mvp_gates = @('T015-T018 product Engine-to-Tessera integration','T020 complete FX-02',
        'T025/T029 temporal reconstruction and real FX-03 replacement','Runtime and full MVP acceptance')
    $report.temporal_scope = 'incomplete temporal coverage declared; complete original fallback validated, no temporal capability claimed'
    $report.original_fixture_scope = 'replay/checkpoint regression; no independent audio-activity oracle or FX-02 closure'
    $exitCode = 0
}
catch {
    $report.errors = @($_.Exception.Message)
    Write-Host ("GATE0_FAILED: {0}" -f $_.Exception.Message) -ForegroundColor Red
}
finally {
    $report.finished_utc = [DateTime]::UtcNow.ToString('o')
    $report.duration_seconds = [Math]::Round(([DateTime]::UtcNow - $started).TotalSeconds, 3)
    New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
    $report | ConvertTo-Json -Depth 15 | Set-Content -LiteralPath $summaryPath -Encoding utf8
    Pop-Location
}
if ($exitCode -eq 0) { Write-Host "GATE0_PASSED: ClownMDEmu seleccionado para la integración posterior; informe: $summaryPath" }
exit $exitCode

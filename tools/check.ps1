[CmdletBinding()]
param(
    [ValidateSet('windows-dev', 'windows-release', 'windows-diagnostics', 'windows-engine-integration', 'windows-runtime-integration', 'windows-clown-reference', 'windows-md-dev')]
    [string]$Preset = 'windows-dev',
    [ValidateSet('All', 'Configure', 'Build', 'Test')]
    [string]$Stage = 'All',
    [ValidateRange(1, 8)]
    [int]$Jobs = 2,
    [string[]]$CMakeArgs = @()
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($env:OS -ne 'Windows_NT') { throw 'WINDOWS_REQUIRED: este check requiere Windows x64.' }
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw 'VSWHERE_MISSING: se requiere Visual Studio 2026 con herramientas C++.' }
$installations = @(& $vswherePath -products '*' -version '[18.0,19.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw 'VSWHERE_FAILED: no se pudo consultar Visual Studio.' }
$installation = $installations | Sort-Object { [version]$_.installationVersion } -Descending | Select-Object -First 1
if (-not $installation) { throw 'VS18_REQUIRED: no se encontró Visual Studio 2026 (18.x) con C++ x64.' }

# Import the x64 compiler environment even when the caller is 32-bit PowerShell.
& (Join-Path $installation.installationPath 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'X64_TOOLCHAIN_REQUIRED: Developer Shell debe seleccionar host y destino x64.'
}
if (-not $env:VCToolsVersion -or [version]$env:VCToolsVersion.TrimEnd('\', '/') -lt [version]'14.51') {
    throw 'MSVC_V145_REQUIRED: se requieren herramientas MSVC 14.51 o posteriores.'
}
$nativeProgramFiles = if ($env:ProgramW6432) { $env:ProgramW6432 } else { $env:ProgramFiles }
$compilerPath = Join-Path $nativeProgramFiles 'LLVM/bin/clang-cl.exe'
if (-not (Test-Path -LiteralPath $compilerPath)) {
    $compilerCommand = Get-Command clang-cl.exe -ErrorAction SilentlyContinue
    if (-not $compilerCommand) { $compilerCommand = Get-Command cl.exe -ErrorAction Stop }
    $compilerPath = $compilerCommand.Source
}
$env:PATH = [IO.Path]::GetDirectoryName($compilerPath) + [IO.Path]::PathSeparator + $env:PATH
foreach ($tool in @('cmake', 'ninja', 'ctest')) { Get-Command $tool -ErrorAction Stop | Out-Null }

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "CHECK_COMMAND_FAILED: $Program terminó con código $LASTEXITCODE." }
}

function Test-Format {
    $clangFormat = Join-Path ([IO.Path]::GetDirectoryName($compilerPath)) 'clang-format.exe'
    if (-not (Test-Path -LiteralPath $clangFormat)) { $clangFormat = (Get-Command clang-format.exe -ErrorAction Stop).Source }
    $files = @(git ls-files 'src/*.cpp' 'src/*.hpp' 'include/*.h' 'tests/*.cpp' 'tests/*.hpp' 'tests/*.c') |
        Where-Object { $_ -notmatch 'fixtures|backend_probe|tessera_observation\.h|clown_state_codec' -and (Test-Path -LiteralPath $_) }
    & $clangFormat --dry-run --Werror @files
    if ($LASTEXITCODE -ne 0) { throw 'FORMAT_VIOLATION: ejecuta clang-format -i sobre los archivos indicados.' }
}

function Test-Tidy {
    $tidy = Join-Path ([IO.Path]::GetDirectoryName($compilerPath)) 'clang-tidy.exe'
    if (-not (Test-Path -LiteralPath $tidy)) { $tidy = (Get-Command clang-tidy.exe -ErrorAction Stop).Source }
    $files = @(git ls-files 'src/*.cpp' 'tools/*.cpp') |
        Where-Object { $_ -notmatch 'clown_state_codec|backend_probe' -and ($Preset -eq 'windows-clown-reference' -or $_ -notmatch 'backends/megadrive') -and (Test-Path -LiteralPath $_) }
    $db = Join-Path $repoRoot "build/$Preset"
    & $tidy -p $db --quiet @files
    if ($LASTEXITCODE -ne 0) { throw 'TIDY_VIOLATION: clang-tidy encontró problemas.' }
}

Push-Location $repoRoot
try {
    & (Join-Path $PSScriptRoot 'bootstrap-dependencies.ps1') -Offline
    Write-Output "Check local: $Preset; etapa $Stage; paralelismo $Jobs."
    if ($Stage -in @('All', 'Configure')) {
        Test-Format
        $configureArguments = @('--preset', $Preset, "-DCMAKE_C_COMPILER=$compilerPath", "-DCMAKE_CXX_COMPILER=$compilerPath") + $CMakeArgs
        Invoke-Checked 'cmake' $configureArguments
        Test-Tidy
    }
    if ($Stage -in @('All', 'Build')) {
        Invoke-Checked 'cmake' @('--build', '--preset', $Preset, '--parallel', $Jobs.ToString())
    }
    if ($Stage -in @('All', 'Test')) {
        Invoke-Checked 'ctest' @('--preset', $Preset, '--parallel', $Jobs.ToString(), '--output-on-failure', '--no-tests=error')
    }
}
finally {
    Pop-Location
}

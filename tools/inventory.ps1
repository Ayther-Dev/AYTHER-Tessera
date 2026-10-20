[CmdletBinding()]
param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../build/evidence'),
    [string]$EnginePrefix = 'C:/ayther/AYTHER-Engine/install/windows-release-engine-vpx'
)
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
if (-not ('TesseraInventoryMemory' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class TesseraInventoryMemory {
 [StructLayout(LayoutKind.Sequential)] public struct Status {
  public uint length, load;
  public ulong totalPhysical, availablePhysical, totalPageFile, availablePageFile;
  public ulong totalVirtual, availableVirtual, availableExtended;
 }
 [DllImport("kernel32.dll", SetLastError=true)] public static extern bool GlobalMemoryStatusEx(ref Status value);
}
'@
}
$memory = New-Object TesseraInventoryMemory+Status
$memory.length = [Runtime.InteropServices.Marshal]::SizeOf($memory)
if (-not [TesseraInventoryMemory]::GlobalMemoryStatusEx([ref]$memory)) { throw 'MEMORY_INVENTORY_FAILED' }
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installations = @(& $vswherePath -all -products '*' -format json | ConvertFrom-Json)
if ($LASTEXITCODE -ne 0) { throw 'VSWHERE_FAILED' }
$vs = $installations | Where-Object { $_.installationVersion -like '18.*' } | Select-Object -First 1
if (-not $vs) { throw 'MSVC_V145_NOT_FOUND' }
& (Join-Path $vs.installationPath 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64' -or $env:VSCMD_ARG_HOST_ARCH -ne 'x64') { throw 'X64_TOOLCHAIN_REQUIRED' }
$vulkanPath = (Get-Command vulkaninfo.exe).Source
$vulkanOutput = @(& $vulkanPath --summary 2>&1 | ForEach-Object { $_.ToString() })
$vulkanExit = $LASTEXITCODE
$vulkanOutput | Set-Content -LiteralPath (Join-Path $outputRoot 'vulkan-summary.txt') -Encoding utf8
$nativeProgramFiles = if ($env:ProgramW6432) { $env:ProgramW6432 } else { $env:ProgramFiles }
$clangPath = Join-Path $nativeProgramFiles 'LLVM/bin/clang-cl.exe'
$enginePrefix = [IO.Path]::GetFullPath($EnginePrefix)
$engineFiles = @('include/ayther/ayther_sdk_version.h','include/ayther/ayther_version.h','lib/ayther_engine.lib','lib/ayther_core.lib','lib/cmake/Ayther/AytherConfig.cmake')
$engineHashes = @($engineFiles | ForEach-Object {
    $path = Join-Path $enginePrefix $_
    [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
})
$versionHeader = Get-Content -LiteralPath (Join-Path $enginePrefix 'include/ayther/ayther_version.h') -Raw
$sdkParts = @('MAJOR','MINOR','PATCH' | ForEach-Object {
    [regex]::Match($versionHeader, ('#define AYTHER_VERSION_{0}\s+(\d+)' -f $_)).Groups[1].Value
})
if (@($sdkParts | Where-Object { $_ -eq '' }).Count) { throw 'SDK_VERSION_NOT_FOUND' }
$os = Get-ItemProperty -LiteralPath 'HKLM:/SOFTWARE/Microsoft/Windows NT/CurrentVersion'
$inventory = [ordered]@{
    schema_version = 1
    captured_utc = [DateTime]::UtcNow.ToString('o')
    os = [ordered]@{ reported_name = $os.ProductName; display_version = $os.DisplayVersion; build = $os.CurrentBuild; revision = $os.UBR; x64 = [Environment]::Is64BitOperatingSystem }
    shell = [ordered]@{ version = $PSVersionTable.PSVersion.ToString(); process_x64 = [Environment]::Is64BitProcess }
    cpu = [ordered]@{ name = Get-ItemPropertyValue -LiteralPath 'HKLM:/HARDWARE/DESCRIPTION/System/CentralProcessor/0' -Name ProcessorNameString; logical_processors = [Environment]::ProcessorCount }
    memory = [ordered]@{ total_bytes = $memory.totalPhysical; available_bytes = $memory.availablePhysical; load_percent = $memory.load }
    disk_free_bytes = (Get-PSDrive -Name C).Free
    toolchain = [ordered]@{ vs_path = $vs.installationPath; vs_version = $vs.installationVersion; host_arch = $env:VSCMD_ARG_HOST_ARCH; target_arch = $env:VSCMD_ARG_TGT_ARCH; msvc_tools = $env:VCToolsVersion; windows_sdk = $env:WindowsSDKVersion; clang_cl = $clangPath; clang_version = @(& $clangPath --version) }
    tools = [ordered]@{ cmake = @(& cmake --version)[0]; ninja = @(& ninja --version)[0]; git = @(& git --version)[0] }
    gpu = [ordered]@{ vulkan_exit_code = $vulkanExit; summary = 'vulkan-summary.txt'; device_names = @($vulkanOutput | Where-Object { $_ -match 'deviceName\s*=' } | ForEach-Object { ($_ -split '=',2)[1].Trim() }); presentation_tested = $false; audio_device_tested = $false }
    engine = [ordered]@{ prefix = $enginePrefix; source_revision_verified = $false; sdk_header_version = ($sdkParts -join '.'); core_abi = [int][regex]::Match($versionHeader, '#define AYTHER_CORE_C_ABI_REVISION\s+(\d+)').Groups[1].Value; crt = '/MD'; files = $engineHashes }
    runtime_lock = Get-Content -LiteralPath 'C:/ayther/AYTHER-Runtime/dependencies/ayther-engine.lock.json' -Raw | ConvertFrom-Json
}
$inventory | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $outputRoot 'environment.json') -Encoding utf8
Write-Output ('Inventario guardado en {0}' -f $outputRoot)

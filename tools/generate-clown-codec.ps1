[CmdletBinding()]
param(
    [string]$SourceRoot = (Join-Path $PSScriptRoot '../third_party/backends/clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d/clownmdemu-core-88ef45a6585556e2247dd26b6c937d6b9fb4a12d'),
    [string]$Clang = 'C:/Program Files/LLVM/bin/clang.exe',
    [switch]$Check
)
$ErrorActionPreference = 'Stop'
# Manual code-generation/audit tool only: Python stdlib is not used by CMake or runtime.
$python = (Get-Command python.exe -ErrorAction Stop).Source
$arguments = @((Join-Path $PSScriptRoot 'generate-clown-codec.py'), '--source-root', $SourceRoot, '--clang', $Clang)
if ($Check) { $arguments += '--check' }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw 'CLOWN_CODEC_GENERATION_FAILED: no se pudo generar o comprobar el codec explícito.' }

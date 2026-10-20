[CmdletBinding()]
param(
    [switch]$Download,
    [switch]$Offline
)

$ErrorActionPreference = 'Stop'
if ($Download -and $Offline) { throw 'BOOTSTRAP_ARGUMENTS_INVALID: Download y Offline son excluyentes.' }
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$lockPath = Join-Path $repoRoot 'cmake/dependencies.lock.json'
$dependencyLock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json
if ($dependencyLock.schema_version -ne 1 -or @($dependencyLock.dependencies).Count -ne 3) {
    throw 'DEPENDENCY_LOCK_INVALID: el lock debe contener las tres dependencias aprobadas.'
}
$verifiedCount = 0
foreach ($dependency in $dependencyLock.dependencies) {
    foreach ($file in $dependency.files) {
        $destination = [IO.Path]::GetFullPath((Join-Path $repoRoot $file.path))
        $allowedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'third_party')) + [IO.Path]::DirectorySeparatorChar
        if (-not $destination.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'DEPENDENCY_PATH_INVALID: una ruta del lock sale de third_party.'
        }
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            if (-not $Download) {
                throw "DEPENDENCY_CACHE_MISSING: falta $($file.path). Ejecuta tools/bootstrap-dependencies.ps1 -Download para descargar explícitamente."
            }
            if ($file.url -notmatch '^https://') { throw 'DEPENDENCY_URL_INVALID: se exige HTTPS.' }
            $directory = [IO.Path]::GetDirectoryName($destination)
            New-Item -ItemType Directory -Force -Path $directory | Out-Null
            $temporary = Join-Path $directory ([IO.Path]::GetFileName($destination) + '.' + [guid]::NewGuid().ToString('N') + '.tmp')
            try {
                Invoke-WebRequest -Uri $file.url -OutFile $temporary
                $downloadHash = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant()
                if ($downloadHash -ne $file.sha256 -or (Get-Item -LiteralPath $temporary).Length -ne $file.bytes) {
                    throw "DEPENDENCY_DOWNLOAD_MISMATCH: la descarga de $($file.path) no coincide con SHA-256 y tamaño fijados."
                }
                Move-Item -LiteralPath $temporary -Destination $destination
            }
            finally {
                if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
            }
        }
        $actualHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualHash -ne $file.sha256 -or (Get-Item -LiteralPath $destination).Length -ne $file.bytes) {
            throw "DEPENDENCY_HASH_MISMATCH: $($file.path) no coincide con el lock; se conserva el archivo para revisión."
        }
        $verifiedCount++
    }
}
Write-Output "Dependencias verificadas: $verifiedCount archivos; tres headers fijados."

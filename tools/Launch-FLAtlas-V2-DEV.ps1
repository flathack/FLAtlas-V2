$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$exePath = Join-Path $repoRoot "build\src\FLAtlas.exe"
$cmakeCachePath = Join-Path $repoRoot "build\CMakeCache.txt"

Set-Location $repoRoot

Write-Host "Building FL Atlas V2 DEV..." -ForegroundColor Cyan
cmake --build build --target FLAtlas --config Debug

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Build failed. Press any key to close." -ForegroundColor Red
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $exePath)) {
    Write-Host ""
    Write-Host "FLAtlas.exe was not found: $exePath" -ForegroundColor Red
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

if (Test-Path -LiteralPath $cmakeCachePath) {
    $cache = Get-Content -LiteralPath $cmakeCachePath
    $qtPrefixLine = $cache | Where-Object { $_ -match '^CMAKE_PREFIX_PATH:.*=(.+)$' } | Select-Object -First 1
    if (-not $qtPrefixLine) {
        $qtPrefixLine = $cache | Where-Object { $_ -match '^Qt6Core_DIR:.*=(.+)/lib/cmake/Qt6Core$' } | Select-Object -First 1
    }
    if ($qtPrefixLine -match '^CMAKE_PREFIX_PATH:.*=(.+)$') {
        $qtPrefix = $Matches[1]
    } elseif ($qtPrefixLine -match '^Qt6Core_DIR:.*=(.+)/lib/cmake/Qt6Core$') {
        $qtPrefix = $Matches[1]
    }

    if ($qtPrefix) {
        $qtBin = Join-Path $qtPrefix "bin"
        if (Test-Path -LiteralPath $qtBin) {
            $env:PATH = "$qtBin;$env:PATH"
            Write-Host "Qt runtime PATH: $qtBin" -ForegroundColor DarkCyan
        }
    }
}

Write-Host "Starting FL Atlas V2..." -ForegroundColor Green
Start-Process -FilePath $exePath -WorkingDirectory (Split-Path -Parent $exePath)

# Zip Duck.vst3 for Windows distribution (unsigned v1).
# Usage (from repo root, after Release build):
#   .\scripts\package_win_duck.ps1
# Optional: $env:AA_DUCK_VERSION = "1.0.0"

$ErrorActionPreference = "Stop"
$version = if ($env:AA_DUCK_VERSION) { $env:AA_DUCK_VERSION } else { "1.0.0" }
$buildDir = if ($env:AA_BUILD_DIR) { $env:AA_BUILD_DIR } else { "build" }
$bundle = Join-Path $buildDir "plugins\duck\Duck_artefacts\Release\VST3\Duck.vst3"
$distDir = Join-Path "dist" "releases\$version"
$zipName = "Duck-$version-win-vst3.zip"
$outZip = Join-Path $distDir $zipName

if (-not (Test-Path $bundle)) {
    Write-Error "VST3 bundle not found: $bundle. Build with: cmake --build build --config Release --target Duck_VST3"
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
if (Test-Path $outZip) { Remove-Item $outZip -Force }
Compress-Archive -Path $bundle -DestinationPath $outZip
Get-FileHash $outZip -Algorithm SHA256 | Format-List

Write-Host ""
Write-Host "Release artifact: $outZip"
Write-Host "Upload to Supabase Storage: duck/$version/$zipName"

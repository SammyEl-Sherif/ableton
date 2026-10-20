# Zip Duck.vst3 for Windows distribution (unsigned v1).
# Usage (from repo root, after Release build):
#   .\scripts\package_win_duck.ps1
# Optional: $env:AA_DUCK_VERSION = "1.0.0"

$ErrorActionPreference = "Stop"
if ($env:AA_DUCK_VERSION) {
    $version = $env:AA_DUCK_VERSION
} else {
    $version = (python scripts/read_project_version.py).Trim()
}
$buildDir = if ($env:AA_BUILD_DIR) { $env:AA_BUILD_DIR } else { "build" }
$bundle = Join-Path $buildDir "plugins\duck\Duck_artefacts\Release\VST3\Duck.vst3"
$distDir = Join-Path "dist" "releases\$version"
$zipName = "Duck-$version-win-vst3.zip"
$outZip = Join-Path $distDir $zipName

if (-not (Test-Path $bundle)) {
    Write-Error "VST3 bundle not found: $bundle. Build with: cmake --build build --config Release --target Duck_VST3"
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$staging = Join-Path $env:TEMP "duck-win-release-$version"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null
Copy-Item -Recurse -Force $bundle (Join-Path $staging "Duck.vst3")
Copy-Item -Force (Join-Path $PSScriptRoot "..\docs\EULA.md") (Join-Path $staging "Animal-Audio-Duck-EULA.txt")
Copy-Item -Force (Join-Path $PSScriptRoot "..\docs\THIRD_PARTY_NOTICES.md") (Join-Path $staging "Third-Party-Notices.txt")
Copy-Item -Force (Join-Path $PSScriptRoot "installers\win\Install-Duck.ps1") (Join-Path $staging "Install-Duck.ps1")
if (Test-Path $outZip) { Remove-Item $outZip -Force }
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $outZip
Remove-Item $staging -Recurse -Force
Get-FileHash $outZip -Algorithm SHA256 | Format-List

Write-Host ""
Write-Host "Release artifact: $outZip"
Write-Host "Upload to Supabase Storage: duck/$version/$zipName"

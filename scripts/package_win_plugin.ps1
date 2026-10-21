# Zip one plugin's VST3 for Windows distribution.
#   .\scripts\package_win_plugin.ps1 -Plugin duck
param(
    [Parameter(Mandatory = $true)]
    [string]$Plugin
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

$metaJson = python -c @"
import json, sys
sys.path.insert(0, 'scripts')
from lib.plugin_release import get_plugin
p = get_plugin('$Plugin')
print(json.dumps({
    'slug': p.slug,
    'version': p.version,
    'product': p.product_name,
    'display': p.display_name,
    'eula': p.eula_file_name,
    'bundle': p.bundle_name,
    'zip': p.win_zip_name(),
    'dist': str(p.release_root()).replace('\\', '/'),
    'bundleRel': p.bundle_build_rel().replace('/', '\\'),
}))
"@
$meta = $metaJson | ConvertFrom-Json

$buildDir = if ($env:AA_BUILD_DIR) { $env:AA_BUILD_DIR } else { "build" }
$bundle = Join-Path $buildDir $meta.bundleRel
$distDir = $meta.dist -replace '/', '\'
$outZip = Join-Path $distDir $meta.zip

if (-not (Test-Path $bundle)) {
    Write-Error "VST3 bundle not found: $bundle. Build Release target ${meta.product}_VST3 first."
}

$eulaName = $meta.eula
$installTemplate = Join-Path $PSScriptRoot "installers\win\Install-Plugin.ps1"
if (-not (Test-Path $installTemplate)) {
    Write-Error "Missing Windows installer template: $installTemplate"
}

New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$staging = Join-Path $env:TEMP "$($meta.slug)-win-release-$($meta.version)"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null
Copy-Item -Recurse -Force $bundle (Join-Path $staging $meta.bundle)
Copy-Item -Force (Join-Path $PSScriptRoot "..\docs\EULA.md") (Join-Path $staging $eulaName)
Copy-Item -Force (Join-Path $PSScriptRoot "..\docs\THIRD_PARTY_NOTICES.md") (Join-Path $staging "Third-Party-Notices.txt")
$installScript = (Get-Content -Raw $installTemplate).Replace('__BUNDLE__', $meta.bundle).Replace('__DISPLAY_NAME__', $meta.display)
[System.IO.File]::WriteAllText((Join-Path $staging "Install-$($meta.product).ps1"), $installScript)
if (Test-Path $outZip) { Remove-Item $outZip -Force }
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $outZip
Remove-Item $staging -Recurse -Force
Get-FileHash $outZip -Algorithm SHA256 | Format-List

Write-Host ""
Write-Host "Release artifact: $outZip"
Write-Host "Upload to Supabase Storage: $($meta.slug)/$($meta.version)/$($meta.zip)"

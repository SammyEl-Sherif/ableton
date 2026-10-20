# Installs IcePenguin.vst3 into the machine-wide VST3 folder (may prompt for administrator approval).
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$bundle = Join-Path $here "IcePenguin.vst3"
$destRoot = ${env:COMMONPROGRAMFILES} + "\VST3"
$dest = Join-Path $destRoot "IcePenguin.vst3"

if (-not (Test-Path $bundle)) {
    Write-Error "IcePenguin.vst3 not found next to this script. Unzip the full download first."
}

if (-not (Test-Path $destRoot)) {
    New-Item -ItemType Directory -Force -Path $destRoot | Out-Null
}

Write-Host "Installing Ice Penguin to $dest"
if (Test-Path $dest) {
    Remove-Item $dest -Recurse -Force
}
Copy-Item -Recurse -Force $bundle $dest
Write-Host ""
Write-Host "Done. Rescan VST3 plug-ins in your DAW."

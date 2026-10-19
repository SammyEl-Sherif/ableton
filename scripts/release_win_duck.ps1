# End-to-end Duck Windows release: build → test (release gate) → zip → validate → optional Supabase upload.
#
#   .\scripts\release_win_duck.ps1
#   .\scripts\release_win_duck.ps1 -Version 1.0.0 -Upload
#   .\scripts\release_win_duck.ps1 -SkipBuild -SkipVerify   # package only
param(
    [string]$Version = $(if ($env:AA_DUCK_VERSION) { $env:AA_DUCK_VERSION } else { "1.0.0" }),
    [switch]$SkipVerify,
    [switch]$SkipBuild,
    [switch]$Upload,
    [string]$BuildDir = $(if ($env:AA_BUILD_DIR) { $env:AA_BUILD_DIR } else { "build" })
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot
$env:AA_DUCK_VERSION = $Version

function Invoke-Step([string]$Label, [scriptblock]$Block) {
    Write-Host ""
    Write-Host "== $Label"
    & $Block
}

if (-not $SkipBuild) {
    Invoke-Step "configure (Release, duck only)" {
        cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release `
            -DAA_PLUGINS=duck -DAA_LOCAL_BUILD=OFF -DAA_BUILD_TESTS=ON
    }
    Invoke-Step "build Duck_VST3 + DuckTests" {
        cmake --build $BuildDir --config Release --target Duck_VST3 DuckTests --parallel
    }
}

if (-not $SkipVerify) {
    Invoke-Step "ctest (release gate, no real-mouse)" {
        ctest --test-dir $BuildDir -C Release --output-on-failure `
            -R "^(dsp|core|duck)\." -E "real mouse"
    }
}

Invoke-Step "package zip" {
    & (Join-Path $PSScriptRoot "package_win_duck.ps1")
}

$artifact = Join-Path $RepoRoot "dist\releases\$Version\Duck-$Version-win-vst3.zip"
$manifest = Join-Path $RepoRoot "dist\releases\$Version\manifest-win.json"
if (-not (Test-Path $artifact)) {
    throw "Release zip not found: $artifact"
}

Invoke-Step "validate" {
    python scripts/validate_duck_release.py --platform win --artifact $artifact `
        --version $Version --allow-unsigned --write-manifest $manifest
}

if ($Upload) {
    Invoke-Step "upload to Supabase" {
        $env:AA_WEB_REPO = if ($env:AA_WEB_REPO) { $env:AA_WEB_REPO } else { Join-Path (Split-Path $RepoRoot -Parent) "animal-audio-web" }
        python scripts/upload_duck_release.py --manifest $manifest --zip $artifact
    }
}

Write-Host ""
Write-Host "Windows release finished."
Write-Host "  zip:      $artifact"
Write-Host "  manifest: $manifest"

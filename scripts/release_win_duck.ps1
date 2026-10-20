# Duck Windows release (wrapper). Prefer: .\scripts\release_win_plugin.ps1 -Plugin duck
param(
    [switch]$SkipVerify,
    [switch]$SkipBuild,
    [switch]$Upload,
    [string]$BuildDir = $(if ($env:AA_BUILD_DIR) { $env:AA_BUILD_DIR } else { "build" })
)
& "$PSScriptRoot\release_win_plugin.ps1" -Plugin duck -BuildDir $BuildDir `
    $(if ($SkipVerify) { '-SkipVerify' }) $(if ($SkipBuild) { '-SkipBuild' }) $(if ($Upload) { '-Upload' })

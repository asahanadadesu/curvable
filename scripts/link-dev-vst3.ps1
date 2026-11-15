# Creates a directory junction in the system VST3 folder that points at the local build,
# so hosts always load the latest build without copying the plugin after every compile.
#
# Usage (run once, as administrator, from the repository root):
#   powershell -ExecutionPolicy Bypass -File scripts\link-dev-vst3.ps1
#
# Parameters:
#   -Config Release|Debug   which build configuration to link (default: Release)
#   -Remove                 remove the link

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [switch]$Remove
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot "build\Curvable_artefacts\$Config\VST3\Curvable.vst3"
$target = Join-Path $env:CommonProgramFiles "VST3\Curvable.vst3"

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Error "Administrator rights are required. Run PowerShell as administrator and try again."
}

if (Test-Path -LiteralPath $target) {
    $item = Get-Item -LiteralPath $target -Force
    if ($item.LinkType -eq "Junction") {
        # Removes only the link, not the build it points to
        $item.Delete()
        Write-Host "Removed the existing link: $target"
    }
    elseif (-not $Remove) {
        Write-Host "A regular folder already exists here (an installed or copied plugin): $target"
        $answer = Read-Host "Delete it and replace it with a link? (y/N)"
        if ($answer -ne "y") { Write-Host "Cancelled."; exit 1 }
        Remove-Item -LiteralPath $target -Recurse -Force
        Write-Host "Deleted: $target"
    }
}

if ($Remove) {
    Write-Host "Done."
    exit 0
}

if (-not (Test-Path -LiteralPath $source)) {
    Write-Error "Build not found: $source`nRun: cmake --build build --config $Config"
}

New-Item -ItemType Junction -Path $target -Target $source | Out-Null
Write-Host "Created link:"
Write-Host "  $target"
Write-Host "  -> $source"

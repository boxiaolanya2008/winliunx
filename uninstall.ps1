<#
.SYNOPSIS
    Remove winlinux and all generated Linux-command shims, and clean the user PATH.

.DESCRIPTION
    Mirrors install.ps1: deletes %LOCALAPPDATA%\winlinux\bin (winlinux.exe + all
    .cmd shims), and removes the bin dir from the current user's PATH.
    Does NOT touch winlinux.exe built in this repo, nor any manually installed files.

.EXAMPLE
    .\uninstall.ps1
    .\uninstall.ps1 -Force
#>
param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$binDir = Join-Path $env:LOCALAPPDATA "winlinux\bin"
$winDir = Split-Path $binDir -Parent

if (-not (Test-Path $binDir)) {
    if (-not $Force) {
        Write-Host "Nothing installed at: $binDir"
        return
    }
}

# 1. Remove bin dir (winlinux.exe + shims)
if (Test-Path $binDir) {
    $count = (Get-ChildItem $binDir -ErrorAction SilentlyContinue | Measure-Object).Count
    Remove-Item -LiteralPath $binDir -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Removed $count file(s) from: $binDir"
} else {
    Write-Host "bin dir not present: $binDir"
}

# 2. Remove empty parent dir if created by us and now empty
if ((Test-Path $winDir) -and -not (Get-ChildItem $winDir -Force -ErrorAction SilentlyContinue)) {
    Remove-Item -LiteralPath $winDir -Force -ErrorAction SilentlyContinue
    Write-Host "Removed empty dir: $winDir"
}

# 3. Prune bin dir from user PATH
try {
    $userPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
} catch {
    $userPath = $null
}
if ($userPath) {
    $entries = $userPath -split ';' | Where-Object { $_ -ne "" }
    $kept = @($entries | Where-Object { $_ -ne $binDir })
    if ($kept.Count -ne $entries.Count -or ($userPath -match [Regex]::Escape($binDir))) {
        [System.Environment]::SetEnvironmentVariable(
            "Path", (($kept | Where-Object { $_ -ne $binDir }) -join ';'), "User")
        Write-Host "Removed from user PATH: $binDir"
    } else {
        Write-Host "bin dir was not in user PATH."
    }
} else {
    Write-Host "No user PATH to clean."
}

Write-Host ""
Write-Host "Uninstall complete. The source repo (build-release\winlinux.exe) is untouched."
Write-Host "Close and reopen terminals to refresh PATH."

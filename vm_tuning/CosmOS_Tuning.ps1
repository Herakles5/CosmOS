# ==========================================================
#  CosmOS Gaming Tuning for Win10_Gaming VM
#  - MSI Mode for NVIDIA GPU (+ HDMI Audio) -> reduces micro-stutter
#  - Power Plan: "Ultimate Performance"
#  - Disable Xbox Game Bar / Game DVR
#  - Disable Background Apps
#  - Enable Windows Game Mode
#  Launched by CosmOS_Tuning.bat with Administrator privileges.
# ==========================================================
$ErrorActionPreference = 'Continue'
Write-Host ""
Write-Host "=== CosmOS Gaming Tuning ===" -ForegroundColor Cyan

function Set-Reg($Path, $Name, $Value, $Type = 'DWord') {
    if (-not (Test-Path $Path)) { New-Item -Path $Path -Force | Out-Null }
    New-ItemProperty -Path $Path -Name $Name -Value $Value -PropertyType $Type -Force | Out-Null
}

# ---------- 1. MSI Mode for NVIDIA GPU + HDMI Audio ----------
Write-Host "`n[1/5] MSI Mode for NVIDIA ..." -ForegroundColor Yellow
$found = 0
$devs = Get-ChildItem 'HKLM:\SYSTEM\CurrentControlSet\Enum\PCI' -ErrorAction SilentlyContinue |
        Where-Object { $_.PSChildName -match '^VEN_10DE&DEV_(1C03|10F1)' }
foreach ($dev in $devs) {
    foreach ($inst in Get-ChildItem $dev.PSPath) {
        $p = Join-Path $inst.PSPath 'Device Parameters\Interrupt Management\MessageSignaledInterruptProperties'
        Set-Reg $p 'MSISupported' 1
        Write-Host "   MSI enabled: $($dev.PSChildName)" -ForegroundColor Green
        $found++
    }
}
if ($found -eq 0) { Write-Host "   No NVIDIA devices found!" -ForegroundColor Red }

# ---------- 2. Power Plan: Ultimate Performance ----------
Write-Host "`n[2/5] Power Plan 'Ultimate Performance' ..." -ForegroundColor Yellow
$ultimate = 'e9a42b02-d5df-448d-aa00-03f14749eb61'
$existing = (powercfg /list) | Select-String $ultimate
if (-not $existing) {
    $out = powercfg -duplicatescheme $ultimate
} else {
    $out = $existing.Line
}
$guid = [regex]::Match(($out | Out-String), '[0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12}').Value
$list = (powercfg /list) | Out-String
$m = [regex]::Matches($list, '([0-9a-fA-F]{8}-(?:[0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12})\s+\((Ultimate Performance|Ultimative Leistung)\)')
if ($m.Count -gt 0) { $guid = $m[0].Groups[1].Value }
if ($guid) {
    powercfg /setactive $guid
    Write-Host "   Active: $guid" -ForegroundColor Green
} else {
    powercfg /setactive SCHEME_MIN   # Fallback: High Performance
    Write-Host "   Fallback: High Performance active" -ForegroundColor Green
}
powercfg /hibernate off 2>$null

# ---------- 3. Disable Xbox Game Bar / Game DVR ----------
Write-Host "`n[3/5] Disabling Xbox Game Bar / Game DVR ..." -ForegroundColor Yellow
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\GameDVR' 'AppCaptureEnabled' 0
Set-Reg 'HKCU:\System\GameConfigStore' 'GameDVR_Enabled' 0
Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'UseNexusForGameBarEnabled' 0
Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'ShowStartupPanel' 0
Set-Reg 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\GameDVR' 'AllowGameDVR' 0
Write-Host "   Done" -ForegroundColor Green

# ---------- 4. Disable Background Apps ----------
Write-Host "`n[4/5] Disabling background apps ..." -ForegroundColor Yellow
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\BackgroundAccessApplications' 'GlobalUserDisabled' 1
Set-Reg 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Search' 'BackgroundAppGlobalToggle' 0
Set-Reg 'HKLM:\SOFTWARE\Policies\Microsoft\Windows\AppPrivacy' 'LetAppsRunInBackground' 2
Write-Host "   Done" -ForegroundColor Green

# ---------- 5. Enable Windows Game Mode ----------
Write-Host "`n[5/5] Enabling Game Mode ..." -ForegroundColor Yellow
Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AutoGameModeEnabled' 1
Set-Reg 'HKCU:\Software\Microsoft\GameBar' 'AllowAutoGameMode' 1
Write-Host "   Done" -ForegroundColor Green

Write-Host "`n=== Complete! Please RESTART Windows (MSI Mode requires reboot) ===" -ForegroundColor Cyan
Write-Host ""
Read-Host "Press Enter to exit"

@echo off
:: CosmOS Gaming Tuning - self-elevates with Administrator privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0CosmOS_Tuning.ps1"

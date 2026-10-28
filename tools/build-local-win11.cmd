@echo off
rem Double-click launcher for build-local-win11.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-local-win11.ps1" %*
pause

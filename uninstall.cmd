@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" goto InteractiveMode

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0InstallScripts\uninstall.ps1" %*
exit /b !ERRORLEVEL!

:InteractiveMode
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0InstallScripts\uninstall.ps1" -Interactive
set "BH_UNINSTALL_EXIT=!ERRORLEVEL!"
if "!BH_UNINSTALL_EXIT!"=="0" goto UninstallSucceeded

echo.
echo BlueprintHelper uninstall failed with exit code !BH_UNINSTALL_EXIT!.
echo Review the error above, then press any key to close this window.
pause >nul
exit /b !BH_UNINSTALL_EXIT!

:UninstallSucceeded
echo.
echo BlueprintHelper uninstall completed successfully.
echo Press any key to close this window.
pause >nul
exit /b 0


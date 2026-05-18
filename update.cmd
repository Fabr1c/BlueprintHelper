@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%~1"=="" goto InteractiveMode

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1" %*
exit /b !ERRORLEVEL!

:InteractiveMode
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update.ps1" -Interactive
set BH_UPDATE_EXIT=!ERRORLEVEL!
if "!BH_UPDATE_EXIT!"=="0" goto UpdateSucceeded

echo.
echo BlueprintHelper update failed with exit code !BH_UPDATE_EXIT!.
echo Press any key to close this window.
pause >nul
exit /b !BH_UPDATE_EXIT!

:UpdateSucceeded
echo.
echo BlueprintHelper update completed successfully.
echo Press any key to close this window.
pause >nul
exit /b 0

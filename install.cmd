@echo off
setlocal EnableExtensions EnableDelayedExpansion
if "%~1"=="" goto InteractiveMode

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0InstallScripts\install.ps1" %*
exit /b !ERRORLEVEL!

:InteractiveMode
set "BH_INSTALL_EXIT=0"
set "BH_INSTALL_TEMP_BASE=%TEMP%\blueprinthelper-install-%RANDOM%-%RANDOM%"
set "BH_INSTALL_DEFAULTS=!BH_INSTALL_TEMP_BASE!.defaults.json"
set "BH_INSTALL_SELECTION=!BH_INSTALL_TEMP_BASE!.selection.json"
set "BH_INSTALL_NODE_LOG=!BH_INSTALL_TEMP_BASE!.node.log"

where node >nul 2>nul
if errorlevel 1 (
  echo BlueprintHelper install failed.
  echo Node.js was not found on PATH.
  set "BH_INSTALL_EXIT=1"
  goto InstallFailed
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0InstallScripts\install.ps1" -WriteNodeDefaults "!BH_INSTALL_DEFAULTS!"
set "BH_INSTALL_EXIT=!ERRORLEVEL!"
if not "!BH_INSTALL_EXIT!"=="0" goto InstallFailed

node "%~dp0InstallScripts\install-prompts.mjs" --raw-only --defaults "!BH_INSTALL_DEFAULTS!" --out "!BH_INSTALL_SELECTION!" 2>"!BH_INSTALL_NODE_LOG!"
set "BH_INSTALL_EXIT=!ERRORLEVEL!"
if "!BH_INSTALL_EXIT!"=="20" goto InstallCancelled
if "!BH_INSTALL_EXIT!"=="10" goto InstallRawUnavailable
if not "!BH_INSTALL_EXIT!"=="0" goto InstallFailed

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0InstallScripts\install.ps1" -SelectionFile "!BH_INSTALL_SELECTION!"
set "BH_INSTALL_EXIT=!ERRORLEVEL!"
if "!BH_INSTALL_EXIT!"=="0" goto InstallSucceeded
goto InstallFailed

:InstallCancelled
call :CleanupTemp
echo.
echo BlueprintHelper install cancelled.
echo Press any key to close this window.
pause >nul
exit /b 20

:InstallRawUnavailable
call :CleanupTemp
echo.
echo BlueprintHelper install could not open the arrow-key form UI in this terminal.
echo Start install.cmd by double-clicking it or from a normal interactive cmd window.
echo Press any key to close this window.
pause >nul
exit /b 10

:InstallFailed
echo.
echo BlueprintHelper install failed with exit code !BH_INSTALL_EXIT!.
if exist "!BH_INSTALL_NODE_LOG!" type "!BH_INSTALL_NODE_LOG!"
call :CleanupTemp
echo Review the error above, then press any key to close this window.
pause >nul
exit /b !BH_INSTALL_EXIT!

:InstallSucceeded
call :CleanupTemp
echo.
echo BlueprintHelper install completed successfully.
echo Press any key to close this window.
pause >nul
exit /b 0

:CleanupTemp
if exist "!BH_INSTALL_DEFAULTS!" del /q "!BH_INSTALL_DEFAULTS!" >nul 2>nul
if exist "!BH_INSTALL_SELECTION!" del /q "!BH_INSTALL_SELECTION!" >nul 2>nul
if exist "!BH_INSTALL_NODE_LOG!" del /q "!BH_INSTALL_NODE_LOG!" >nul 2>nul
exit /b 0

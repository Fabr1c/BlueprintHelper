@echo off
setlocal EnableExtensions

call "%~dp0update.cmd" %*
exit /b %ERRORLEVEL%

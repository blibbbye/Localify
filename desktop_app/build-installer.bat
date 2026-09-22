@echo off
setlocal
title Localify Desktop - Windows Build
cd /d "%~dp0"
echo.
echo === Localify Desktop ===
echo.
where node >nul 2>nul
if errorlevel 1 (
  echo ERROR: Node.js was not found.
  echo Install Node.js 20+ and run this file again.
  echo.
  pause
  exit /b 1
)

if not exist node_modules (
  echo Installing Electron and build dependencies...
  call npm install --no-fund --no-audit
  if errorlevel 1 (
    echo.
    echo ERROR: npm install failed.
    echo.
    pause
    exit /b 1
  )
)

echo.
echo Building Windows installer and portable EXE...
call npm run dist
set "EXITCODE=%ERRORLEVEL%"
echo.
if not "%EXITCODE%"=="0" (
  echo BUILD FAILED. The window will stay open so you can read the error.
  echo.
  pause
  exit /b %EXITCODE%
)

echo BUILD COMPLETE.
echo.
echo Installer and portable EXE are in the release folder.
echo.
explorer "%~dp0release"
pause

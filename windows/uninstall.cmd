@echo off
setlocal
echo =============================================
echo   GoTiengViet - Complete Uninstall Script
echo =============================================
echo.
echo This script removes ALL traces of GoTiengViet:
echo   - Running processes
echo   - Installation directory
echo   - Configuration files
echo   - TSF registry keys (HKLM + HKCU)
echo   - Vietnamese language from Windows
echo.
echo MUST Run as Administrator!
echo =============================================
echo.

whoami /groups | findstr /i "S-1-16-12288" >nul
if %errorlevel% neq 0 (
    echo [ERROR] This script must be run as Administrator!
    echo         Right-click and select "Run as administrator"
    echo.
    pause
    exit /b 1
)

set "INSTALL_DIR=%LOCALAPPDATA%\Programs\GoTiengViet"
set "CONFIG_DIR=%LOCALAPPDATA%\GoTiengViet"
set "CONFIG_DIR2=%APPDATA%\GoTiengViet"
set "CLSID={E3B0C442-98FC-4F2E-9C8F-7B2A3E1D4C5B}"

echo [1/7] Killing processes...
taskkill /f /im explorer.exe 2>nul
taskkill /f /im gotiengviet.exe 2>nul
taskkill /f /im gtv_tsf.dll 2>nul
taskkill /f /im ctfmon.exe 2>nul
timeout /t 3 /nobreak >nul
echo    [OK]

echo [2/7] Deleting install directory: %INSTALL_DIR%
rmdir /s /q "%INSTALL_DIR%" 2>nul
if exist "%INSTALL_DIR%" (
    echo    Taking ownership...
    takeown /f "%INSTALL_DIR%" /r /d y 2>nul
    icacls "%INSTALL_DIR%" /grant administrators:F /t 2>nul
    rmdir /s /q "%INSTALL_DIR%" 2>nul
)
if not exist "%INSTALL_DIR%" (echo    [OK]) else (echo    [FAIL] - reboot and run again)

echo [3/7] Deleting config directories...
rmdir /s /q "%CONFIG_DIR%" 2>nul
rmdir /s /q "%CONFIG_DIR2%" 2>nul
if not exist "%CONFIG_DIR%" (echo    [OK]) else (echo    [FAIL])

echo [4/7] Deleting HKLM GoTV CTF TIP registry...
reg delete "HKLM\SOFTWARE\Microsoft\CTF\TIP\%CLSID%" /f 2>nul
reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\%CLSID%" 2>nul
if %errorlevel% neq 0 (echo    [OK]) else (echo    [FAIL])

echo [5/7] Deleting HKCU GoTV CTF TIP registry...
reg delete "HKCU\SOFTWARE\Microsoft\CTF\TIP\%CLSID%" /f 2>nul
echo    [OK]

echo [6/7] Removing Vietnamese language from Windows...
powershell -Command "Set-WinUserLanguageList en-US -Force" 2>nul
echo    [OK]

echo [7/7] Restarting explorer...
start explorer.exe
timeout /t 3 /nobreak >nul
echo    [OK]

echo.
echo =============================================
echo   VERIFICATION
echo =============================================

if not exist "%INSTALL_DIR%" (echo   [OK] Install dir deleted) else (echo   [FAIL] Install dir still exists)
if not exist "%CONFIG_DIR%" (echo   [OK] Config dir deleted) else (echo   [FAIL] Config dir still exists)

reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\%CLSID%" 2>nul
if %errorlevel% neq 0 (echo   [OK] HKLM GoTV TIP removed) else (echo   [FAIL] HKLM GoTV TIP still exists)

reg query "HKCU\SOFTWARE\Microsoft\CTF\TIP\%CLSID%" 2>nul
if %errorlevel% neq 0 (echo   [OK] HKCU GoTV TIP removed) else (echo   [FAIL] HKCU GoTV TIP still exists)

echo.
echo   Language list:
powershell -Command "Get-WinUserLanguageList | ForEach-Object { Write-Host ('   ' + $_.LanguageTag + ' - ' + ($_.InputMethodTips -join ',')) }" 2>nul

echo.
echo =============================================
echo   Cleanup complete! Please REBOOT before
echo   reinstalling GoTiengViet.
echo =============================================
echo.
pause

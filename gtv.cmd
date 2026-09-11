@echo off
REM GoTiengViet entry point for cmd.exe: forwards to gtv.sh (needs MSYS2/Git bash).
REM Usage: gtv <build|test|vet|clean|install|install-ollama|uninstall|package|bump|help>
REM NOTE: plain `where bash` may resolve to the WSL stub, so real
REM installations are probed first.
setlocal
set "HERE=%~dp0"
if exist "C:\msys64\usr\bin\bash.exe" (
  "C:\msys64\usr\bin\bash.exe" "%HERE%gtv.sh" %*
  exit /b %ERRORLEVEL%
)
if exist "C:\Program Files\Git\bin\bash.exe" (
  "C:\Program Files\Git\bin\bash.exe" "%HERE%gtv.sh" %*
  exit /b %ERRORLEVEL%
)
where bash >nul 2>nul
if %ERRORLEVEL%==0 (
  bash "%HERE%gtv.sh" %*
  exit /b %ERRORLEVEL%
)
echo gtv.cmd: bash not found. Install MSYS2 or Git for Windows, then run: gtv build
exit /b 1

@echo off
rem RoboticelaOS - build the Stage 01 bootloader from the repository root.
rem Delegates to build-bootloader.sh (Git Bash / MSYS2 bash required).
setlocal
set "HERE=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
    echo ERROR: bash not found. Use Git Bash or MSYS2 to run this script. >&2
    exit /b 1
)
bash "%HERE%build-bootloader.sh" %*

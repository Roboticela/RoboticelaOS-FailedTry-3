@echo off
rem RoboticelaOS - run the Stage 01 bootloader in QEMU from the repository root.
rem Delegates to run-bootloader.sh (Git Bash / MSYS2 bash required).
rem Options: --headless | --screenshot
setlocal
set "HERE=%~dp0"
where bash >nul 2>nul
if errorlevel 1 (
    echo ERROR: bash not found. Use Git Bash or MSYS2 to run this script. >&2
    exit /b 1
)
bash "%HERE%run-bootloader.sh" %*

@echo off
rem RoboticelaOS - build the Stage 01 bootloader from the repository root.
rem Locates a real bash (Git Bash / MSYS2) and delegates to build-bootloader.sh.
setlocal EnableExtensions
set "HERE=%~dp0"

rem 1) Known install locations (preferred; works even if bash is not on PATH).
set "BASH_EXE="
if not defined BASH_EXE if exist "%ProgramFiles%\Git\bin\bash.exe" set "BASH_EXE=%ProgramFiles%\Git\bin\bash.exe"
if not defined BASH_EXE if exist "%ProgramFiles(x86)%\Git\bin\bash.exe" set "BASH_EXE=%ProgramFiles(x86)%\Git\bin\bash.exe"
if not defined BASH_EXE if exist "%SystemDrive%\msys64\usr\bin\bash.exe" set "BASH_EXE=%SystemDrive%\msys64\usr\bin\bash.exe"
if not defined BASH_EXE if exist "%ProgramFiles%\msys64\usr\bin\bash.exe" set "BASH_EXE=%ProgramFiles%\msys64\usr\bin\bash.exe"

rem 2) Fall back to PATH, skipping the WSL bash.exe stubs (System32/WindowsApps)
rem    that fail with "execvpe(/bin/bash) failed" when no WSL distro is installed.
if not defined BASH_EXE (
    for /f "delims=" %%B in ('where bash 2^>nul') do (
        if not defined BASH_EXE (
            echo %%B | findstr /i /c:"System32" /c:"WindowsApps" >nul
            if errorlevel 1 set "BASH_EXE=%%B"
        )
    )
)

if not defined BASH_EXE (
    echo ERROR: no Git Bash or MSYS2 bash found. >&2
    echo Install Git for Windows or MSYS2, or add a real bash to PATH. >&2
    exit /b 1
)

"%BASH_EXE%" "%HERE%build-bootloader.sh" %*

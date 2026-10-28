@echo off
setlocal
if not exist "%~dp0generated\outputs\Release\CG2_00_01.exe" (
    echo Release executable not found. Build CG2_00_01 in Release x64 first.
    pause
    exit /b 1
)
start "" /d "%~dp0project" "%~dp0generated\outputs\Release\CG2_00_01.exe"
exit /b %errorlevel%

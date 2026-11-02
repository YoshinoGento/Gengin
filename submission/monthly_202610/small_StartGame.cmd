@echo off
setlocal
if not exist "%~dp0Runtime\SuiroNogyo.exe" goto missing
if not exist "%~dp0Runtime\dxcompiler.dll" goto missing
if not exist "%~dp0Runtime\dxil.dll" goto missing
if not exist "%~dp0project\Resources\levels\farm_scene.json" goto missing
start "" /d "%~dp0project" "%~dp0Runtime\SuiroNogyo.exe"
exit /b %errorlevel%
:missing
echo Game files are missing. Extract the entire ZIP before starting.
echo If the executable was cleaned, run BuildRelease.cmd first.
pause
exit /b 1

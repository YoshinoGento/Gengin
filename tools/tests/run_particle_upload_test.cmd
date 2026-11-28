@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "obj=generated\codex_checks\particle_upload"
if not exist "%obj%" mkdir "%obj%"
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject\engine tools\tests\particle_upload_test.cpp /Fo:%obj%\ /Fe:%obj%\check.exe
if errorlevel 1 exit /b %errorlevel%
"%obj%\check.exe"
exit /b %errorlevel%

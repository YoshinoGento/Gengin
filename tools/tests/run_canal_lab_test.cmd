@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b 1
set "o=generated\codex_checks\canal_lab"
if not exist "%o%" mkdir "%o%"
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /DNOMINMAX /Iproject\application tools\tests\canal_lab_test.cpp project\application\farm\lab\CanalLabSystem.cpp /Fo:%o%\ /Fe:%o%\test.exe
if errorlevel 1 exit /b 1
"%o%\test.exe"
exit /b %errorlevel%

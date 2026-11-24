@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "o=generated\codex_checks\brush_play_tests"
if not exist "%o%" mkdir "%o%"
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /DNDEBUG /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_brush_play_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmBrushPlaySystem.cpp /Fo:%o%\ /Fe:%o%\test.exe
if errorlevel 1 exit /b %errorlevel%
"%o%\test.exe"
exit /b %errorlevel%

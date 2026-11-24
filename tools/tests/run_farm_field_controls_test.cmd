@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "out=generated\codex_checks\field_controls_obj"
if not exist "%out%" mkdir "%out%"
set "sys=project\application\farm\system"
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_field_controls_test.cpp project\application\farm\core\FarmGrid.cpp %sys%\FarmInputSystem.cpp %sys%\FarmToolSystem.cpp %sys%\FarmToolBehavior.cpp %sys%\FarmToolActionSystem.cpp %sys%\FarmCropQualitySystem.cpp %sys%\FarmEconomySystem.cpp project\engine\command\CommandHistory.cpp /Fo:%out%\ /Fe:%out%\test.exe
if errorlevel 1 exit /b %errorlevel%
"%out%\test.exe"

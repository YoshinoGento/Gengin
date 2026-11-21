@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\input_visibility_obj mkdir generated\codex_checks\input_visibility_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_input_visibility_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmVisualSystem.cpp project\application\farm\system\FarmIrrigationSystem.cpp project\application\farm\system\FarmTerrainQuerySystem.cpp /Fo:generated\codex_checks\input_visibility_obj\ /Fe:generated\codex_checks\farm_input_visibility_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_input_visibility_test.exe

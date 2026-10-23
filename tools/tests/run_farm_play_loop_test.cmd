@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\play_loop_obj mkdir generated\codex_checks\play_loop_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_play_loop_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\application\farm\system\FarmGrowthSystem.cpp project\application\farm\system\FarmToolActionSystem.cpp project\application\farm\system\FarmToolBehavior.cpp project\application\farm\system\FarmProgressionSystem.cpp project\engine\command\CommandHistory.cpp /Fo:generated\codex_checks\play_loop_obj\ /Fe:generated\codex_checks\farm_play_loop_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_play_loop_test.exe

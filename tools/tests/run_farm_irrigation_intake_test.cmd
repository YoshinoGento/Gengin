@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\intake_obj mkdir generated\codex_checks\intake_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /Fo:generated\codex_checks\intake_obj\ /Iproject\application /Iproject\engine tools\tests\farm_irrigation_intake_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmIrrigationSystem.cpp project\application\farm\system\FarmIrrigationPreviewSystem.cpp project\application\farm\system\FarmToolActionSystem.cpp project\application\farm\system\FarmToolBehavior.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\application\farm\system\FarmGrowthComparisonSystem.cpp project\engine\command\CommandHistory.cpp /Fe:generated\codex_checks\farm_irrigation_intake_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_irrigation_intake_test.exe

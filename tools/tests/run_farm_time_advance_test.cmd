@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\time_advance_obj mkdir generated\codex_checks\time_advance_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /DNDEBUG /W4 /WX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_time_advance_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmDateSystem.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\application\farm\system\FarmGrowthSystem.cpp project\application\farm\system\FarmGrowthComparisonSystem.cpp project\application\farm\system\FarmIrrigationSystem.cpp project\application\farm\system\FarmProgressionSystem.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\time_advance_obj\ /Fe:generated\codex_checks\farm_time_advance_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_time_advance_test.exe
exit /b %errorlevel%

@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "obj=generated\codex_checks\water_trial_obj"
if not exist "%obj%" mkdir "%obj%"
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject /Iproject\application /Iproject\engine tools\tests\water_trial_test.cpp project\application\farm\lab\WaterTrialSystem.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\application\farm\system\FarmGrowthSystem.cpp project\application\farm\system\FarmIrrigationSystem.cpp project\application\farm\system\FarmToolActionSystem.cpp project\application\farm\system\FarmToolBehavior.cpp project\engine\command\CommandHistory.cpp /Fo:%obj%\ /Fe:%obj%\trial.exe
if errorlevel 1 exit /b %errorlevel%
"%obj%\trial.exe"
exit /b %errorlevel%

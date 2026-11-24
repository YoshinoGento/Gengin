@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "o=generated\codex_checks\action_particles"
if not exist "%o%" mkdir "%o%"
set "s=project\application\farm\system"
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /DNDEBUG /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine /Iproject\externals\DirectXTex /Iproject\externals\DirectXTex\DirectXTex tools\tests\farm_action_particles_test.cpp project\application\farm\core\FarmGrid.cpp %s%\FarmActionParticles.cpp %s%\FarmVisualSystem.cpp %s%\FarmToolActionSystem.cpp %s%\FarmToolBehavior.cpp %s%\FarmCropQualitySystem.cpp %s%\FarmIrrigationSystem.cpp %s%\FarmEconomySystem.cpp project\engine\command\CommandHistory.cpp project\engine\base\Logger.cpp /Fo:%o%\ /Fe:%o%\farm_action_particles_test.exe
if errorlevel 1 exit /b %errorlevel%
"%o%\farm_action_particles_test.exe"
exit /b %errorlevel%

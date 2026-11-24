@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "o=generated\codex_checks\water_play_tests"
if not exist "%o%" mkdir "%o%"
set "s=project\application\farm\system"
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /DNDEBUG /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine /Iproject\externals\DirectXTex /Iproject\externals\DirectXTex\DirectXTex tools\tests\farm_water_play_test.cpp project\application\farm\core\FarmGrid.cpp %s%\FarmWaterPlaySystem.cpp %s%\FarmVisualSystem.cpp %s%\FarmIrrigationSystem.cpp project\engine\base\Logger.cpp /Fo:%o%\ /Fe:%o%\test.exe
if errorlevel 1 exit /b %errorlevel%
"%o%\test.exe"
exit /b %errorlevel%

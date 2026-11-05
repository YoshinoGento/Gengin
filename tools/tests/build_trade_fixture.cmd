@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\trade_fixture_obj mkdir generated\codex_checks\trade_fixture_obj
cl /nologo /std:c++20 /utf-8 /O2 /DNDEBUG /EHsc /W4 /WX /D NOMINMAX /Iproject /Iproject\application /Iproject\engine tools\tests\build_trade_fixture.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmDocumentSystem.cpp project\application\farm\system\FarmDateSystem.cpp project\application\farm\system\FarmProgressionSystem.cpp project\application\farm\system\FarmCropSelectionSystem.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\engine\io\JsonFile.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\trade_fixture_obj\ /Fe:generated\codex_checks\build_trade_fixture.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\build_trade_fixture.exe

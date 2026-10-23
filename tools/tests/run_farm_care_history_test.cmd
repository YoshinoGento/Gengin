@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\care_history_obj mkdir generated\codex_checks\care_history_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /D NOMINMAX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_care_history_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmCropSelectionSystem.cpp project\application\farm\system\FarmDateSystem.cpp project\application\farm\system\FarmDocumentSystem.cpp project\application\farm\system\FarmProgressionSystem.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\application\farm\system\FarmToolActionSystem.cpp project\application\farm\system\FarmToolBehavior.cpp project\engine\command\CommandHistory.cpp project\engine\io\JsonFile.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\care_history_obj\ /Fe:generated\codex_checks\farm_care_history_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_care_history_test.exe

@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\exhibition_obj mkdir generated\codex_checks\exhibition_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine /Iproject\externals\DirectXTex /Iproject\externals\DirectXTex\DirectXTex tools\tests\farm_exhibition_test.cpp project\application\farm\system\FarmExhibitionSystem.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmDateSystem.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\application\farm\system\FarmProgressionSystem.cpp project\application\farm\system\FarmDocumentSystem.cpp project\application\farm\system\FarmCropSelectionSystem.cpp project\engine\io\JsonFile.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\exhibition_obj\ /Fe:generated\codex_checks\farm_exhibition_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_exhibition_test.exe
exit /b %errorlevel%

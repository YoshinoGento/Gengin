@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\contest_detail_obj mkdir generated\codex_checks\contest_detail_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine /Iproject\externals\DirectXTex /Iproject\externals\DirectXTex\DirectXTex tools\tests\farm_contest_presentation_test.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp /Fo:generated\codex_checks\contest_detail_obj\ /Fe:generated\codex_checks\farm_contest_presentation_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_contest_presentation_test.exe
exit /b %errorlevel%

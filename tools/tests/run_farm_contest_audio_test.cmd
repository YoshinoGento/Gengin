@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\contest_audio_obj mkdir generated\codex_checks\contest_audio_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject /Iproject\application /Iproject\engine /Iproject\externals\DirectXTex /Iproject\externals\DirectXTex\DirectXTex tools\tests\farm_contest_audio_test.cpp project\application\farm\system\FarmContestAudioSystem.cpp project\engine\audio\Audio.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\contest_audio_obj\ /Fe:generated\codex_checks\farm_contest_audio_test.exe /link ole32.lib
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_contest_audio_test.exe
exit /b %errorlevel%

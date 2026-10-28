@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /W4 /WX /MT /Iproject\externals\assimp\include tools\tests\contest_ceremony_asset_test.cpp /Fo:generated\codex_checks\contest_ceremony_asset_test.obj /Fe:generated\codex_checks\contest_ceremony_asset_test.exe /link /LIBPATH:project\externals\assimp\lib\Release assimp-vc143-mt.lib
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\contest_ceremony_asset_test.exe
exit /b %errorlevel%

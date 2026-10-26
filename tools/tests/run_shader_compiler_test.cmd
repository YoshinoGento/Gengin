@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\shader_compiler_obj mkdir generated\codex_checks\shader_compiler_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject\engine tools\tests\shader_compiler_test.cpp project\engine\base\ShaderCompiler.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\shader_compiler_obj\ /Fe:generated\codex_checks\shader_compiler_test.exe /link dxcompiler.lib
if errorlevel 1 exit /b %errorlevel%
copy /Y "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxcompiler.dll" generated\codex_checks\dxcompiler.dll >nul
copy /Y "C:\Program Files (x86)\Windows Kits\10\Redist\D3D\x64\dxil.dll" generated\codex_checks\dxil.dll >nul
generated\codex_checks\shader_compiler_test.exe

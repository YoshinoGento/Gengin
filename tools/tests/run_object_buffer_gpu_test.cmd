@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\object_buffer_obj mkdir generated\codex_checks\object_buffer_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject\engine tools\tests\object_buffer_gpu_test.cpp /Fo:generated\codex_checks\object_buffer_obj\ /Fe:generated\codex_checks\object_buffer_gpu_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\object_buffer_gpu_test.exe

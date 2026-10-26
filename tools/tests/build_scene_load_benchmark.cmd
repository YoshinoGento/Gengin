@echo off
setlocal
set "CG2_ORIGINAL_PATH=%PATH%"
set Path=
set "PATH=%CG2_ORIGINAL_PATH%"
cd /d "%~dp0..\..\project"
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" CG2_00_01.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /p:PostBuildEventUseInBuild=false /p:ForceImportAfterCppTargets="%~dp0scene_load_benchmark.targets" /m /v:minimal /nologo
exit /b %errorlevel%

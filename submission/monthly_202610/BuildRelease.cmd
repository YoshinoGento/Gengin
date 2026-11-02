@echo off
setlocal
set "MONTHLY_PATH=%PATH%"
set Path=
set "PATH=%MONTHLY_PATH%"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto missing
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\Current\Bin\MSBuild.exe`) do set "BUILD=%%I"
if not defined BUILD goto missing
pushd "%~dp0project"
"%BUILD%" CG2_00_01.sln /t:Build /p:Configuration=Release /p:Platform=x64 /p:CL_MPCount=2 /m:2 /nr:false /v:minimal /nologo
set "RESULT=%errorlevel%"
popd
if not "%~1"=="nopause" pause
exit /b %RESULT%
:missing
echo Visual Studio with the v145 C++ toolset and Windows SDK is required.
if not "%~1"=="nopause" pause
exit /b 1

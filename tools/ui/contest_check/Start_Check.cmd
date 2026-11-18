@echo off
setlocal
pushd "%~dp0" || exit /b 1
start "" /wait "%~dp0SuiroNogyo.exe"
set "result=%errorlevel%"
popd
exit /b %result%

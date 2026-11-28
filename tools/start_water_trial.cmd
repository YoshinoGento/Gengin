@echo off
setlocal
cd /d "%~dp0..\project"
set "SUIRO_WATER_TRIAL=1"
set "SUIRO_WATER_TRIAL_COMPARE="
set "SUIRO_WATER_TRIAL_CLASSIC=0"
set "SUIRO_WATER_TRIAL_PREVIOUS=0"
if /i "%~3"=="shaded" set "SUIRO_WATER_TRIAL_PREVIOUS=1"
set "SUIRO_WATER_TRIAL_LEGACY=0"
if /i "%~3"=="classic" set "SUIRO_WATER_TRIAL_CLASSIC=1"
if /i "%~4"=="legacy" set "SUIRO_WATER_TRIAL_LEGACY=1"
set "SUIRO_WATER_TRIAL_INSPECT="
if /i "%~1"=="inspect" (
  set "SUIRO_WATER_TRIAL_COMPARE=C"
  set "SUIRO_WATER_TRIAL_INSPECT=1"
)
if /i "%~1"=="compare" (
  set "SUIRO_WATER_TRIAL_COMPARE=A"
  if /i "%~2"=="B" set "SUIRO_WATER_TRIAL_COMPARE=B"
  if /i "%~2"=="C" set "SUIRO_WATER_TRIAL_COMPARE=C"
)
if not exist "..\generated\outputs\Release\CG2_00_01.exe" (
  echo Build Release x64 before launching the water trial.
  exit /b 1
)
start "Water Trial" "..\generated\outputs\Release\CG2_00_01.exe"

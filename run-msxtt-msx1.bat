@echo off
setlocal
set ROOT=%~dp0
set OMSXDIR=%ROOT%tools\openmsxnet
set DSK=%ROOT%emul\dsk\msxtt.dsk

set OPENMSX_HOME=%ROOT%emul\openmsxnet-home
set OPENMSX_USER_DATA=%OPENMSX_HOME%\share

if not exist "%OMSXDIR%\openmsx.exe" (
  echo openMSXnet not found: %OMSXDIR%\openmsx.exe
  exit /b 1
)
if not exist "%DSK%" (
  echo Disk image not found: %DSK%
  echo Run build.bat first.
  exit /b 1
)
if not exist "%OPENMSX_USER_DATA%" mkdir "%OPENMSX_USER_DATA%"

cd /d "%OMSXDIR%"
:: MSX1 + mapper so Nextor can run. Detection should take the Screen 2 path.
start "" openmsx.exe -machine Canon_V-20 -ext MemoryMapper -ext Nextor213_IDE -ext unapinet -diska "%DSK%"

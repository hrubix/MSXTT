@echo off
setlocal
set ROOT=%~dp0
set OMSXDIR=%ROOT%tools\openmsxnet
set DSK=%ROOT%emul\dsk\MSXTT.DSK

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
start "" openmsx.exe -machine Philips_NMS_8250-16 -ext Nextor213_IDE -ext unapinet -diska "%DSK%"

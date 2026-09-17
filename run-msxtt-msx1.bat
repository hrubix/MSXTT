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
:: National CF-3300 (MSX1, built-in FDC). Slot expander + 512K mapper so
:: Nextor + unapinet both fit. Detection takes the Screen 2 path.
start "" openmsx.exe -machine National_CF-3300 -ext slotexpander -ext ram512k -ext Nextor213_IDE -ext unapinet -diska "%DSK%"

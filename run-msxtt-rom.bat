@echo off
setlocal
set ROOT=%~dp0
set OMSXDIR=%ROOT%tools\openmsxnet
set ROM=%ROOT%emul\rom\MSXTT.ROM

set OPENMSX_HOME=%ROOT%emul\openmsxnet-home
set OPENMSX_USER_DATA=%OPENMSX_HOME%\share

if not exist "%OMSXDIR%\openmsx.exe" (
  echo openMSXnet not found: %OMSXDIR%\openmsx.exe
  exit /b 1
)
if not exist "%ROM%" (
  echo ROM not found: %ROM%
  echo Run build-msxtt-rom.bat first.
  exit /b 1
)
if not exist "%OPENMSX_USER_DATA%" mkdir "%OPENMSX_USER_DATA%"

cd /d "%OMSXDIR%"
:: Page-2 cart + unapinet I/O bridge. Full UNAPI still needs a Pico+/UNAPI
:: implementation in another slot (real hardware) — the openMSX unapinet
:: extension alone is I/O; DOS uses UNAPI.COM as the EXTBIO TSR.
start "" openmsx.exe -machine Philips_NMS_8250-16 -ext unapinet -carta "%ROM%"

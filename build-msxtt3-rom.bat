:: MSXTT3.ROM — canonical ROM UNAPI discovery, splash diagnostics
@echo off
setlocal
if not defined MSXGL_PATH set MSXGL_PATH=MSXgl

set "PATH=%CD%\%MSXGL_PATH%\tools\sdcc\bin;%PATH%"

cls
"%MSXGL_PATH%\tools\build\Node\node.exe" "%MSXGL_PATH%\engine\script\js\build.js" projname=msxttrom3 %*
if errorlevel 1 exit /b 1
if not exist build mkdir build
if not exist emul\rom mkdir emul\rom

if exist emul\rom\msxttrom3.rom (
	copy /Y emul\rom\msxttrom3.rom build\MSXTT3.ROM >nul
) else if exist out\msxttrom3.rom (
	copy /Y out\msxttrom3.rom build\MSXTT3.ROM >nul
) else (
	echo ROM not found
	exit /b 1
)

copy /Y build\MSXTT3.ROM emul\rom\MSXTT3.ROM >nul
pushd emul\rom
if exist msxttrom3.rom del /Q msxttrom3.rom
popd
if exist out\msxttrom3.rom del /Q out\msxttrom3.rom
echo Wrote build\MSXTT3.ROM and emul\rom\MSXTT3.ROM

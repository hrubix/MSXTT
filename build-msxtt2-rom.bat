:: MSXTT2.ROM — debug Pico+ UNAPI viewer
@echo off
setlocal
if not defined MSXGL_PATH set MSXGL_PATH=MSXgl

set "PATH=%CD%\%MSXGL_PATH%\tools\sdcc\bin;%PATH%"

cls
"%MSXGL_PATH%\tools\build\Node\node.exe" "%MSXGL_PATH%\engine\script\js\build.js" projname=msxttrom2 %*
if errorlevel 1 exit /b 1
if not exist build mkdir build
if not exist emul\rom mkdir emul\rom

if exist emul\rom\msxttrom2.rom (
	copy /Y emul\rom\msxttrom2.rom build\MSXTT2.ROM >nul
) else if exist out\msxttrom2.rom (
	copy /Y out\msxttrom2.rom build\MSXTT2.ROM >nul
) else (
	echo ROM not found
	exit /b 1
)

copy /Y build\MSXTT2.ROM emul\rom\MSXTT2.ROM >nul
pushd emul\rom
if exist msxttrom2.rom del /Q msxttrom2.rom
popd
if exist out\msxttrom2.rom del /Q out\msxttrom2.rom
echo Wrote build\MSXTT2.ROM and emul\rom\MSXTT2.ROM

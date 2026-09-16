:: MSXTT.ROM — Pico+/UNAPI TLS viewer at 8000h (not part of build.bat)
@echo off
setlocal
if not defined MSXGL_PATH set MSXGL_PATH=MSXgl

set "PATH=%CD%\%MSXGL_PATH%\tools\sdcc\bin;%PATH%"

cls
"%MSXGL_PATH%\tools\build\Node\node.exe" "%MSXGL_PATH%\engine\script\js\build.js" projname=msxttrom %*
if errorlevel 1 exit /b 1
if not exist build mkdir build
if not exist emul\rom mkdir emul\rom

if exist emul\rom\msxttrom.rom (
	copy /Y emul\rom\msxttrom.rom build\MSXTT.ROM >nul
) else if exist out\msxttrom.rom (
	copy /Y out\msxttrom.rom build\MSXTT.ROM >nul
) else (
	echo ROM not found
	exit /b 1
)

copy /Y build\MSXTT.ROM emul\rom\MSXTT.ROM >nul
pushd emul\rom
if exist msxttrom.rom del /Q msxttrom.rom
popd
if exist out\msxttrom.rom del /Q out\msxttrom.rom
echo Wrote build\MSXTT.ROM and emul\rom\MSXTT.ROM

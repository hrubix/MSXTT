:: MSXTTPP.COM — Pico+ DOS2 viewer, direct NOS TLS (not part of build.bat)
@echo off
setlocal
if not defined MSXGL_PATH set MSXGL_PATH=MSXgl

set "PATH=%CD%\%MSXGL_PATH%\tools\sdcc\bin;%PATH%"

"%MSXGL_PATH%\tools\build\Node\node.exe" "%MSXGL_PATH%\engine\script\js\build.js" projname=msxttpp %*
if errorlevel 1 exit /b 1

if not exist build mkdir build
if exist out\msxttpp.com (
	copy /Y out\msxttpp.com build\MSXTTPP.COM >nul
) else if exist emul\dos2\msxttpp.com (
	copy /Y emul\dos2\msxttpp.com build\MSXTTPP.COM >nul
) else (
	echo MSXTTPP.COM not found
	exit /b 1
)
echo Wrote build\MSXTTPP.COM

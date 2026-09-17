:: MSXTT — public MSXgl DOS2 build (MSXgl/ inside this project)
@echo off
setlocal
if not defined MSXGL_PATH set MSXGL_PATH=MSXgl

set "PATH=%CD%\%MSXGL_PATH%\tools\sdcc\bin;%PATH%"

"%MSXGL_PATH%\tools\build\Node\node.exe" "%MSXGL_PATH%\engine\script\js\build.js" projname=msxtt %*
if errorlevel 1 exit /b 1

if not exist build mkdir build
if exist out\msxtt.com (
	copy /Y out\msxtt.com build\MSXTT.COM >nul
) else if exist emul\dos2\msxtt.com (
	copy /Y emul\dos2\msxtt.com build\MSXTT.COM >nul
)
if exist build\MSXTT.COM echo Wrote build\MSXTT.COM

call "%~dp0tools\rebuild-dsk.bat"

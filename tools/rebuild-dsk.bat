@echo off
setlocal
set ROOT=%~dp0..
set DOS=%ROOT%\emul\dos2
set TAR=%ROOT%\MSXgl\tools\build\msxtar
if not exist "%DOS%\msxtt.com" (
  echo Missing %DOS%\msxtt.com — run build.bat
  exit /b 1
)
copy /Y "%ROOT%\disk\AUTOEXEC.BAT" "%TAR%\autoexec.bat" >nul
copy /Y "%ROOT%\disk\AUTOEXEC.BAT" "%DOS%\AUTOEXEC.BAT" >nul
copy /Y "%ROOT%\disk\AUTOEXEC.BAT" "%DOS%\autoexec.bat" >nul
copy /Y "%ROOT%\disk\TT.CFG" "%TAR%\TT.CFG" >nul
copy /Y "%DOS%\msxtt.com" "%TAR%\msxtt.com" >nul
copy /Y "%ROOT%\disk\UNAPI.COM" "%TAR%\UNAPI.COM" >nul
if not exist "%DOS%\COMMAND2.COM" copy /Y "%ROOT%\tools\dos2\COMMAND2.COM" "%DOS%\COMMAND2.COM" >nul
if not exist "%DOS%\MSXDOS2.SYS" copy /Y "%ROOT%\tools\dos2\MSXDOS2.SYS" "%DOS%\MSXDOS2.SYS" >nul
copy /Y "%DOS%\COMMAND2.COM" "%TAR%\COMMAND2.COM" >nul
copy /Y "%DOS%\MSXDOS2.SYS" "%TAR%\MSXDOS2.SYS" >nul
if not exist "%ROOT%\emul\dsk" mkdir "%ROOT%\emul\dsk"
if not exist "%ROOT%\build" mkdir "%ROOT%\build"
pushd "%TAR%"
msxtar -cf temp.dsk --dos2 --size=720K autoexec.bat msxtt.com COMMAND2.COM MSXDOS2.SYS UNAPI.COM TT.CFG
copy /Y temp.dsk "%ROOT%\emul\dsk\MSXTT.DSK" >nul
copy /Y temp.dsk "%ROOT%\build\MSXTT.DSK" >nul
del /Q UNAPI.COM >nul
copy /Y "%ROOT%\disk\AUTOEXEC.NOUNAPI.BAT" autoexec.bat >nul
msxtar -cf temp.dsk --dos2 --size=720K autoexec.bat msxtt.com COMMAND2.COM MSXDOS2.SYS TT.CFG
copy /Y temp.dsk "%ROOT%\emul\dsk\msxtt-nounapi.dsk" >nul
del /Q temp.dsk autoexec.bat msxtt.com COMMAND2.COM MSXDOS2.SYS TT.CFG 2>nul
popd
echo Wrote %ROOT%\emul\dsk\MSXTT.DSK
echo Wrote %ROOT%\build\MSXTT.DSK
echo Wrote %ROOT%\emul\dsk\msxtt-nounapi.dsk

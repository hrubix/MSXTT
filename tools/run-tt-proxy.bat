@echo off
setlocal
cd /d "%~dp0.."

:: openMSXnet is a Windows process: it must reach 127.0.0.1:8080 on the
:: Windows network stack. A proxy that only listens inside WSL is invisible
:: to the emulator when localhost forwarding is off (common on WSL2).
:: Prefer native Windows Python; use --wsl only if you need the old path.

if /I "%~1"=="--wsl" (
	shift
	goto :run_wsl
)

where python >nul 2>&1
if %errorlevel%==0 goto :run_win
where py >nul 2>&1
if %errorlevel%==0 goto :run_win_py
goto :run_wsl

:run_win
:: Free a leftover WSL listener so Windows can bind the same port.
where wsl >nul 2>&1 && wsl -e bash -lc "fuser -k 8080/tcp >/dev/null 2>&1 || true"
for /f "tokens=5" %%p in ('netstat -ano ^| findstr /R /C:":8080 .*LISTENING"') do (
	taskkill /F /PID %%p >nul 2>&1
)
echo Starting Teletekst proxy with Windows Python (reachable from openMSX)...
python tools\tt-proxy.py %*
goto :eof

:run_win_py
where wsl >nul 2>&1 && wsl -e bash -lc "fuser -k 8080/tcp >/dev/null 2>&1 || true"
for /f "tokens=5" %%p in ('netstat -ano ^| findstr /R /C:":8080 .*LISTENING"') do (
	taskkill /F /PID %%p >nul 2>&1
)
echo Starting Teletekst proxy with Windows Python (reachable from openMSX)...
py -3 tools\tt-proxy.py %*
goto :eof

:run_wsl
where wsl >nul 2>&1
if errorlevel 1 (
	echo No Windows Python or WSL found to run tools\tt-proxy.py
	exit /b 1
)
wsl -e bash -lc "fuser -k 8080/tcp >/dev/null 2>&1 || true"
echo Starting Teletekst proxy inside WSL.
echo WARNING: openMSX on Windows often cannot reach WSL :8080 unless
echo localhost forwarding / mirrored networking is enabled.
wsl --cd "%CD%" -e python3 tools/tt-proxy.py %*

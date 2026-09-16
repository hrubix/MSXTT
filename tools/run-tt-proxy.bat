@echo off
setlocal
cd /d "%~dp0.."
:: openMSXnet on Windows often cannot complete TCP to a native Win32 listener
:: on 127.0.0.1. Prefer WSL python3 so localhost forwarding works in the emulator.
where wsl >nul 2>&1
if %errorlevel%==0 (
	wsl --cd "%CD%" -e python3 tools/tt-proxy.py %*
	goto :eof
)
python tools\tt-proxy.py %*

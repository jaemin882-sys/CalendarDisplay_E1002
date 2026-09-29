@echo off
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel% equ 0 (py -3 tools\setup_config.py) else (python tools\setup_config.py)
pause

@echo off
cd /d "%~dp0"
echo E1002 actual iCloud calendar preview
echo.
python tools\preview.py
if errorlevel 1 pause

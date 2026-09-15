@echo off
:: Double-click to start the kimodo.cpp browser demo
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0demo.ps1" %*
pause

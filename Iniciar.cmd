@echo off
set "APP=%~dp0build\windows\Release\AlbionAssistant.exe"
if not exist "%APP%" (
  echo Execute scripts\build-windows.ps1 -Configuration Release antes de iniciar.
  pause
  exit /b 1
)
start "" "%APP%"

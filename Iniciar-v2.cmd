@echo off
set "APP=%~dp0build\windows\GuidedRelease\AlbionAssistant.exe"
if not exist "%APP%" (
  echo Execute scripts\build-windows.ps1 -Configuration Release -BuildDirectory build/windows/GuidedRelease antes de iniciar.
  pause
  exit /b 1
)
start "" "%APP%"

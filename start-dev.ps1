$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$backendDir = Join-Path $root "backend"
$frontendDir = Join-Path $root "frontend"
$backendExe = Join-Path $backendDir "second_brain_server.exe"

Write-Host "Starting AI Second Brain development stack..." -ForegroundColor Cyan

if (-not (Test-Path $backendExe)) {
    Write-Host "Backend executable not found. Building with g++..." -ForegroundColor Yellow
    Push-Location $backendDir
    g++ -std=c++20 -I. -I..\llama.cpp\vendor server.cpp core\ai_engine.cpp services\notes_service.cpp services\ai_service.cpp -lws2_32 -o second_brain_server.exe
    Pop-Location
}

Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-Command",
    "Set-Location '$backendDir'; .\second_brain_server.exe"
)

Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-Command",
    "Set-Location '$frontendDir'; npm run dev -- --host 127.0.0.1 --port 5173"
)

Write-Host "Backend:  http://127.0.0.1:8080" -ForegroundColor Green
Write-Host "Frontend: http://127.0.0.1:5173" -ForegroundColor Green
Write-Host "Two new PowerShell windows were started." -ForegroundColor Green

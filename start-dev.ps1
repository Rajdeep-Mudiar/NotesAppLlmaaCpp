$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$backendDir  = Join-Path $root "backend"
$frontendDir = Join-Path $root "frontend"
$backendExe  = Join-Path $backendDir "second_brain_server.exe"

# Absolute paths
$llamaDir          = Join-Path $root "llama.cpp\build\bin\Debug"
$llamaServerBinary = Join-Path $llamaDir "llama-server.exe"
$llamaCliBinary    = Join-Path $llamaDir "llama-cli.exe"
$modelPath         = Join-Path $root "models\model.gguf"
$dataDir           = Join-Path $root "data\notes"

Write-Host "Starting AI Second Brain development stack..." -ForegroundColor Cyan

# 1. Compile backend if needed
if (-not (Test-Path $backendExe)) {
    Write-Host "Backend executable not found. Building with g++..." -ForegroundColor Yellow
    Push-Location $backendDir
    g++ -std=c++20 -I. -I"..\llama.cpp\vendor" `
        server.cpp `
        core\ai_engine.cpp `
        services\notes_service.cpp `
        services\ai_service.cpp `
        -lws2_32 `
        -o second_brain_server.exe
    Pop-Location
    if (-not (Test-Path $backendExe)) {
        Write-Host "Build failed! Check g++ output above." -ForegroundColor Red
        exit 1
    }
    Write-Host "Build OK." -ForegroundColor Green
}

# 2. Start llama-server
Write-Host ""
Write-Host "Starting llama-server on port 8081..." -ForegroundColor Yellow
Write-Host "(The model will load once; all queries after that are fast)" -ForegroundColor Gray

$llamaServerCmd = "Set-Location '$llamaDir'; " +
    ".\llama-server.exe -m '$modelPath' " +
    "--host 127.0.0.1 --port 8081 " +
    "-ngl 0 " +
    "-c 2048 " +
    "-np 1 " +
    "--log-disable"

Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-Command",
    $llamaServerCmd
)

# Wait until llama-server is ready
Write-Host "Waiting for llama-server to load model (this takes ~30-90 s once)..." -ForegroundColor Yellow
$maxWaitSec = 180
$elapsed    = 0
$ready      = $false

while ($elapsed -lt $maxWaitSec) {
    Start-Sleep -Seconds 3
    $elapsed += 3
    try {
        $resp = Invoke-WebRequest -Uri "http://127.0.0.1:8081/health" `
                    -TimeoutSec 2 -ErrorAction Stop
        $health = $resp.Content | ConvertFrom-Json -ErrorAction SilentlyContinue
        if ($resp.StatusCode -eq 200 -and $health.status -eq "ok") {
            Write-Host "llama-server is READY!" -ForegroundColor Green
            $ready = $true
            break
        } else {
            Write-Host "  llama-server status: $($health.status) ($elapsed s)..." -ForegroundColor Gray
        }
    } catch {
        Write-Host "  Still loading... ($elapsed s)" -ForegroundColor Gray
    }
}

if (-not $ready) {
    Write-Host "Warning: llama-server did not become ready in $maxWaitSec s." -ForegroundColor Red
    Write-Host "         Backend will fall back to slow llama-cli mode for now." -ForegroundColor Red
}

# 3. Start the C++ backend server
Write-Host ""
Write-Host "Launching C++ Backend (port 8080)..." -ForegroundColor Gray

$backendCmd = "`$env:SECOND_BRAIN_PORT = '8080'; " +
    "`$env:SECOND_BRAIN_DATA_DIR = '$dataDir'; " +
    "`$env:SECOND_BRAIN_LLAMA_BINARY = '$llamaCliBinary'; " +
    "`$env:SECOND_BRAIN_MODEL_PATH = '$modelPath'; " +
    "`$env:SECOND_BRAIN_LLAMA_SERVER_PORT = '8081'; " +
    "Set-Location '$backendDir'; .\second_brain_server.exe"

Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-Command",
    $backendCmd
)

# 4. Start the Vite frontend
Write-Host "Launching Frontend (port 5173)..." -ForegroundColor Gray

Start-Process powershell -ArgumentList @(
    "-NoExit",
    "-Command",
    "`$env:VITE_API_BASE_URL = 'http://127.0.0.1:8080'; Set-Location '$frontendDir'; npm run dev -- --host 127.0.0.1 --port 5173"
)

Write-Host ""
Write-Host "Stack is starting up!" -ForegroundColor Green
Write-Host ""
Write-Host "  llama-server : http://127.0.0.1:8081" -ForegroundColor Cyan
Write-Host "  Backend      : http://127.0.0.1:8080" -ForegroundColor Cyan
Write-Host "  Frontend     : http://127.0.0.1:5173" -ForegroundColor Cyan
Write-Host ""
Write-Host "Performance tip: the FIRST query may take ~30 s to load the model." -ForegroundColor Yellow
Write-Host "All subsequent queries will respond in 1-5 seconds." -ForegroundColor Green

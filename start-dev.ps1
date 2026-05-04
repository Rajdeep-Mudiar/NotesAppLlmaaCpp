$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$backendDir  = Join-Path $root "backend"
$frontendDir = Join-Path $root "frontend"
$backendExe  = Join-Path $backendDir "second_brain_server.exe"

# 0. Detect Paths for llama.cpp binaries
$llamaBase = Join-Path $root "llama.cpp\build\bin"
$llamaDir = ""
$candidates = @("Debug", "Release", ".")

foreach ($c in $candidates) {
    $path = Join-Path $llamaBase $c
    if (Test-Path (Join-Path $path "llama-server.exe")) {
        $llamaDir = $path
        break
    }
}

if ($llamaDir -eq "") {
    Write-Host "Error: llama-server.exe not found in $llamaBase (checked Debug, Release, and bin)." -ForegroundColor Red
    Write-Host "Please build llama.cpp first using CMake." -ForegroundColor Yellow
    exit 1
}

$llamaServerBinary = Join-Path $llamaDir "llama-server.exe"
$llamaCliBinary    = Join-Path $llamaDir "llama-cli.exe"
$modelPath         = Join-Path $root "models\model.gguf"
$dataDir           = Join-Path $root "data\notes"

# Find best python
$PYTHON_CMD = "python"
if (Test-Path "$root\.venv\Scripts\python.exe") {
    $PYTHON_CMD = "$root\.venv\Scripts\python.exe"
}

Write-Host "Starting AI Second Brain development stack..." -ForegroundColor Cyan

# 0. Clean up any existing processes to ensure new settings (like context size) apply
Write-Host "Cleaning up old processes..." -ForegroundColor Gray
Stop-Process -Name "second_brain_server" -ErrorAction SilentlyContinue
Stop-Process -Name "llama-server" -ErrorAction SilentlyContinue

# 1. Environment & Dependency Checks
Write-Host "Checking environment..." -ForegroundColor Gray

if (-not (Test-Path (Join-Path $backendDir ".env"))) {
    Write-Host "Warning: backend\.env file is missing. The server might fail to connect to MongoDB." -ForegroundColor Yellow
}

if (-not (Test-Path $modelPath)) {
    Write-Host "Error: Model file not found at $modelPath" -ForegroundColor Red
    Write-Host "Please download a GGUF model and place it in the models/ folder." -ForegroundColor Yellow
    exit 1
}

# Check Python dependencies
Write-Host "Checking Python dependencies..." -ForegroundColor Gray
$oldPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $PYTHON_CMD -c "import pymongo, dotenv" 2>$null
$pythonExit = $LASTEXITCODE
$ErrorActionPreference = $oldPreference

if ($pythonExit -ne 0) {
    Write-Host "Error: Missing Python modules in your environment ($PYTHON_CMD)." -ForegroundColor Red
    Write-Host "Attempting to install missing dependencies..." -ForegroundColor Yellow
    & $PYTHON_CMD -m pip install pymongo python-dotenv
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to install dependencies. Please run manually: pip install pymongo python-dotenv" -ForegroundColor Red
        exit 1
    }
}

# Check Frontend dependencies
if (-not (Test-Path (Join-Path $frontendDir "node_modules"))) {
    Write-Host "node_modules missing in frontend. Running npm install..." -ForegroundColor Yellow
    Push-Location $frontendDir
    cmd /c npm install
    Pop-Location
}

# 2. Compile backend if needed (check source timestamps)
$srcFiles = Get-ChildItem -Path $backendDir -Filter "*.cpp" -Recurse
$latestSrc = $srcFiles | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$needsBuild = $false

if (-not (Test-Path $backendExe)) {
    $needsBuild = $true
} elseif ($latestSrc.LastWriteTime -gt (Get-Item $backendExe).LastWriteTime) {
    Write-Host "Source files changed. Rebuilding backend..." -ForegroundColor Yellow
    $needsBuild = $true
}

if ($needsBuild) {
    Write-Host "Building C++ Backend with g++..." -ForegroundColor Yellow
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

# 3. Start llama-server
Write-Host ""
Write-Host "Starting llama-server on port 8081..." -ForegroundColor Yellow

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$llamaArgs = "-m `"$modelPath`" --host 127.0.0.1 --port 8081 -c 2048 -ngl 0 -np 1"

Start-Process -FilePath $llamaServerBinary -ArgumentList $llamaArgs -WorkingDirectory $llamaDir -WindowStyle Normal

# Wait until llama-server is ready
Write-Host "Waiting for llama-server to load model..." -ForegroundColor Yellow
$maxWaitSec = 180
$elapsed    = 0
$ready      = $false

while ($elapsed -lt $maxWaitSec) {
    Start-Sleep -Seconds 3
    $elapsed += 3
    try {
        $resp = Invoke-WebRequest -Uri "http://127.0.0.1:8081/health" `
                    -TimeoutSec 2 -UseBasicParsing -ErrorAction Stop
        $health = $resp.Content | ConvertFrom-Json -ErrorAction SilentlyContinue
        if ($resp.StatusCode -eq 200 -and $health.status -eq "ok") {
            Write-Host "llama-server is READY!" -ForegroundColor Green
            $ready = $true
            break
        }
    } catch {
        Write-Host "  Still loading... ($elapsed s)" -ForegroundColor Gray
    }
}

# 4. Start the C++ backend server
Write-Host ""
Write-Host "Launching C++ Backend (port 8080)..." -ForegroundColor Gray

$env:SECOND_BRAIN_PORT = "8080"
$env:SECOND_BRAIN_DATA_DIR = $dataDir
$env:SECOND_BRAIN_LLAMA_BINARY = $llamaCliBinary
$env:SECOND_BRAIN_MODEL_PATH = $modelPath
$env:SECOND_BRAIN_LLAMA_SERVER_PORT = "8081"
$env:SECOND_BRAIN_PYTHON = $PYTHON_CMD
$env:PYTHONIOENCODING = "utf-8"

Start-Process -FilePath $backendExe -WorkingDirectory $backendDir -WindowStyle Normal

# 5. Start the Vite frontend
Write-Host "Launching Frontend (port 5173)..." -ForegroundColor Gray

$env:VITE_API_BASE_URL = "http://127.0.0.1:8080"
# Use cmd /c for better compatibility with npm (which is a .cmd file)
Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "npm", "run", "dev", "--", "--host", "127.0.0.1", "--port", "5173" -WorkingDirectory $frontendDir -WindowStyle Normal

Write-Host ""
Write-Host "Stack is starting up!" -ForegroundColor Green
Write-Host ""
Write-Host "  llama-server : http://127.0.0.1:8081" -ForegroundColor Cyan
Write-Host "  Backend      : http://127.0.0.1:8080" -ForegroundColor Cyan
Write-Host "  Frontend     : http://127.0.0.1:5173" -ForegroundColor Cyan
Write-Host ""

# Automatically open the browser
Write-Host "Opening frontend in your browser..." -ForegroundColor Gray
Start-Sleep -Seconds 2
Start-Process "http://127.0.0.1:5173"

Write-Host "Performance tip: the FIRST query may take ~30 s to load the model." -ForegroundColor Yellow
Write-Host "All subsequent queries will respond in 1-5 seconds." -ForegroundColor Green

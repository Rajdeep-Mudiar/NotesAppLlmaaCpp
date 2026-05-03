# AI Second Brain Notes Application

A privacy-focused, full-stack notes app with local AI features. Built for speed, privacy, and extensibility.

**Tech Stack:**

- Frontend: React + Vite (JSX)
- Backend: Pure C++ (custom HTTP server)
- Local AI: [llama.cpp](https://github.com/ggerganov/llama.cpp)
- Storage: File-based JSON notes with version history

---

## Project Structure

- `backend/` — C++ API server, RAG logic, note storage
- `frontend/` — React UI
- `data/notes/` — note files (`.json`) and version snapshots
- `models/` — local GGUF model files

---

## Features

- Add, edit, delete notes
- Auto-tagging for notes
- RAG chat (`/search`) and streaming chat (`/search/stream`)
- Flashcards, knowledge graph, contradictions, ideas, learning path
- Note version history and restore

---

## Prerequisites

### Windows

- Node.js 18+
- npm 9+
- g++ with C++20 support (MinGW recommended)
- CMake (optional, for llama.cpp)

### Linux

- Node.js 18+
- npm 9+
- g++
- cmake, make

---

## Required Environment Variables

### Backend

| Variable                    | Required               | Default             | Description                  |
| --------------------------- | ---------------------- | ------------------- | ---------------------------- |
| `SECOND_BRAIN_PORT`         | No                     | `8080`              | Backend port                 |
| `SECOND_BRAIN_DATA_DIR`     | No                     | `data/notes`        | Notes storage directory      |
| `SECOND_BRAIN_LLAMA_BINARY` | Yes (for AI responses) | `llama-cli`         | Path to llama.cpp CLI binary |
| `SECOND_BRAIN_MODEL_PATH`   | Yes (for AI responses) | `models/model.gguf` | Path to GGUF model           |

### Frontend

| Variable            | Required | Default                 | Description      |
| ------------------- | -------- | ----------------------- | ---------------- |
| `VITE_API_BASE_URL` | No       | `http://127.0.0.1:8080` | Backend base URL |

---

## Installation & Local Development

### 1. Clone the repository

```bash
git clone <your-repo-url>
cd NotesAppLlmaaCpp
```

### 2. Get llama.cpp (if missing)

```bash
git clone https://github.com/ggerganov/llama.cpp.git
# or
git clone https://github.com/ggml-org/llama.cpp.git
```

### 3. Build llama.cpp CLI

```bash
cd llama.cpp
cmake -S . -B build
cmake --build build -j
# On Windows, use CMake GUI or run in Developer PowerShell if needed
```

The binary will be at:

- Linux/macOS: `llama.cpp/build/bin/llama-cli`
- Windows: `llama.cpp/build/bin/llama-cli.exe`

### 4. Place your model

Download a GGUF model and place it at `models/model.gguf` or set `SECOND_BRAIN_MODEL_PATH` to your model location.

### 5. Build and run the backend

#### Windows (PowerShell)

```powershell
cd backend
g++ -std=c++20 -I. -I..\llama.cpp\vendor server.cpp core\ai_engine.cpp services\notes_service.cpp services\ai_service.cpp -lws2_32 -o second_brain_server.exe

# Set environment variables (edit paths as needed)
$env:SECOND_BRAIN_PORT="8080"
$env:SECOND_BRAIN_DATA_DIR="..\data\notes"
$env:SECOND_BRAIN_LLAMA_BINARY="..\llama.cpp\build\bin\llama-cli.exe"
$env:SECOND_BRAIN_MODEL_PATH="..\models\model.gguf"

.\second_brain_server.exe
```

#### Linux

```bash
cd backend
g++ -std=c++20 -I. -I../llama.cpp/vendor server.cpp core/ai_engine.cpp services/notes_service.cpp services/ai_service.cpp -o second_brain_server

export SECOND_BRAIN_PORT=8080
export SECOND_BRAIN_DATA_DIR=../data/notes
export SECOND_BRAIN_LLAMA_BINARY=../llama.cpp/build/bin/llama-cli
export SECOND_BRAIN_MODEL_PATH=../models/model.gguf

./second_brain_server
```

### 6. Install and run the frontend

In a new terminal:

```bash
cd frontend
npm install
npm run dev
```

Frontend: [http://127.0.0.1:5173](http://127.0.0.1:5173)
Backend: [http://127.0.0.1:8080](http://127.0.0.1:8080)

### 7. One-command start (Windows)

```powershell
.\start-dev.ps1
```

---

## API Endpoints

- `GET /health` — Health check
- `GET /notes` — List notes
- `POST /add` — Add note
- `POST /update` — Update note
- `POST /delete` — Delete note
- `POST /search` — RAG search
- `POST /search/stream` — Streaming search (SSE)
- `POST /insights` — Insights
- `POST /flashcards` — Flashcards
- `POST /graph` — Knowledge graph
- `POST /contradictions` — Contradictions
- `POST /learning-path` — Learning path
- `POST /questions` — Questions
- `POST /ideas` — Ideas
- `POST /history` — Note history
- `POST /restore-version` — Restore note version

---

## Deployment

### Backend on Render

Recommended: Deploy as Docker Web Service.

1. Create Render Web Service from your GitHub repo.
2. Use Docker runtime with `backend/` as root directory.
3. Add persistent disk mounted at `/var/data`.
4. Add env vars:

```
SECOND_BRAIN_PORT=10000
SECOND_BRAIN_DATA_DIR=/var/data/notes
SECOND_BRAIN_LLAMA_BINARY=/opt/llama.cpp/build/bin/llama-cli
SECOND_BRAIN_MODEL_PATH=/var/data/models/model.gguf
```

5. Ensure model file exists at `/var/data/models/model.gguf`.
6. Expose and verify `https://<render-service>.onrender.com/health`.

**Note:** Large GGUF models may exceed Render free/low-tier resources.

### Frontend on Vercel

1. Import GitHub repo in Vercel.
2. Set project root to `frontend`.
3. Build settings:

- Build command: `npm run build`
- Output directory: `dist`

4. Add env var:

```
VITE_API_BASE_URL=https://<your-render-backend>.onrender.com
```

5. Deploy.

---

## Troubleshooting

- **Frontend port conflict:**
  - Run: `npm run dev -- --port 5174`
- **Backend returns fallback answer only:**
  - Check `SECOND_BRAIN_LLAMA_BINARY` and `SECOND_BRAIN_MODEL_PATH` are correct and model exists
- **Streaming not visible:**
  - Ensure frontend uses `/search/stream`
  - Ensure reverse proxy disables buffering for SSE

---

## License

This project is for educational and personal use. See LICENSE for details.

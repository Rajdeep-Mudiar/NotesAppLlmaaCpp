# AI Second Brain Notes Application

A privacy-focused, full-stack notes app with local AI features and cloud persistence. Built for speed, privacy, and extensibility.

**Tech Stack:**

- **Frontend:** React + Vite (Modern JSX)
- **Backend:** Pure C++20 (Custom HTTP/Socket server)
- **Database:** MongoDB (via Python Storage Bridge)
- **Local AI:** [llama.cpp](https://github.com/ggerganov/llama.cpp) (Streaming SSE & RAG)
- **Orchestration:** PowerShell / Bash

---

## Project Structure

- `backend/` — C++ API server and RAG logic.
- `frontend/` — React UI.
- `llama.cpp/` — Submodule/Folder for the inference engine.
- `models/` — Local GGUF model files.
- `data/` — Local cache and temporary storage.

---

## Features

- **Note Management:** Add, edit, delete notes with auto-tagging.
- **RAG Chat:** Chat with your notes using local LLMs.
- **Streaming UI:** Real-time AI responses via Server-Sent Events (SSE).
- **Advanced Insights:** Knowledge graphs, flashcards, contradiction detection, and learning paths.
- **Version History:** Snapshotting and restoration of previous note versions.
- **Cloud Persistence:** Securely sync notes to MongoDB Atlas or a local instance.

---

## Prerequisites

### General
- **Node.js 18+** & **npm 9+**
- **Python 3.10+** (with `pymongo` and `python-dotenv`)
- **g++** with C++20 support (MinGW-w64 recommended on Windows)
- **CMake** (required for building llama.cpp)
- **MongoDB** (Atlas connection string or local instance)

### Windows
- [Git for Windows](https://git-scm.com/)
- [MinGW-w64](https://www.mingw-w64.org/) or MSVC
- [CMake](https://cmake.org/download/)

---

## Environment Variables

Create a `.env` file in the `backend/` directory:

| Variable | Required | Description |
| :--- | :--- | :--- |
| `MONGO_DB_URL` | **Yes** | Your MongoDB connection string. |
| `SECOND_BRAIN_LLAMA_BINARY` | **Yes** | Path to `llama-cli.exe` or `llama-server.exe`. |
| `SECOND_BRAIN_MODEL_PATH` | **Yes** | Path to your `.gguf` model file. |
| `SECOND_BRAIN_PORT` | No | Backend port (Default: `8080`). |
| `SECOND_BRAIN_PYTHON` | No | Path to python executable if not in PATH. |
| `VITE_API_BASE_URL` | No | Frontend API endpoint (Default: `http://127.0.0.1:8080`). |

---

## How to Run

### 1. Initial Setup
```bash
# Clone the repository
git clone <your-repo-url>
cd NotesAppLlmaaCpp

# Install Python dependencies
pip install pymongo python-dotenv
```

### 2. Build llama.cpp
```bash
cd llama.cpp
cmake -S . -B build
cmake --build build --config Debug -j # Or Release
```
 then activate the venv from root directory
 .\.venv\Scripts\Activate.ps1
 
### 3. Quick Start (Windows)
The easiest way to start the entire stack is using the provided PowerShell script:
```powershell
.\start-dev.ps1
```
This script will:
- Check/Compile the C++ backend.
- Launch `llama-server` on port 8081.
- Start the C++ Backend on port 8080.
- Start the Vite Frontend on port 5173.

### 4. Manual Start (Cross-Platform)

#### A. Start the Backend
```bash
cd backend
# Compile (example for Windows/MinGW)
g++ -std=c++20 -I. -I..\llama.cpp\vendor server.cpp core\ai_engine.cpp services\notes_service.cpp services\ai_service.cpp -lws2_32 -o second_brain_server.exe

# Run (Ensure .env is configured)
.\second_brain_server.exe
```

#### B. Start the Frontend
```bash
cd frontend
npm install
npm run dev
```

---

## API Endpoints

- `GET /health` — System status.
- `GET /notes` — Fetch all notes from MongoDB.
- `POST /add` — Create a new note.
- `POST /search` — AI search/chat (Standard).
- `POST /search/stream` — AI search/chat (Streaming SSE).
- `POST /insights` — Generate note metrics.
- `POST /flashcards` — Generate study cards from notes.
- `POST /graph` — Get Knowledge Graph data.

---

## Troubleshooting

- **"ModuleNotFoundError: No module named 'pymongo'"**:
  Run `pip install pymongo python-dotenv`.
- **Backend fails to connect to MongoDB**:
  Check your `MONGO_DB_URL` in `backend/.env`. Ensure your IP is whitelisted in MongoDB Atlas.
- **AI responses are slow or empty**:
  The first query usually takes longer as the model loads into memory. Ensure `SECOND_BRAIN_MODEL_PATH` points to a valid `.gguf` file.
- **C++ Build Errors**:
  Ensure you have a modern compiler supporting C++20. Use `g++ --version` to check.

---

## License

Personal and Educational use only. Built with ❤️ for the AI Second Brain community.

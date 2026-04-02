# AI Second Brain Notes Application

Local full-stack notes app with a pure C++ socket backend and a React/Vite frontend.

## Structure

- `backend/` - C++ server, note storage, RAG logic, and llama.cpp bridge
- `frontend/` - React UI built with Vite and JSX components
- `backend/data/notes/` - file-based note storage
- `models/` - place your local GGUF model here

## Backend

The backend uses:

- custom HTTP routing on raw sockets
- `std::filesystem` for note storage
- `popen()` to call a local llama.cpp binary
- SSE chat streaming via `POST /search/stream`
- note version history via snapshots in `data/notes/.history/`

Environment variables:

- `SECOND_BRAIN_PORT` - backend port, defaults to `8080`
- `SECOND_BRAIN_DATA_DIR` - override notes storage directory
- `SECOND_BRAIN_LLAMA_BINARY` - path to the llama.cpp CLI executable
- `SECOND_BRAIN_MODEL_PATH` - path to the GGUF model file

Build from `backend/` with CMake:

```bash
cmake -S . -B build
cmake --build build
```

Or compile directly with g++ on Windows:

```bash
g++ -std=c++20 -I. -I..\llama.cpp\vendor server.cpp core\ai_engine.cpp services\notes_service.cpp services\ai_service.cpp -lws2_32 -o second_brain_server.exe
```

Key API routes:

- `POST /add` - add note
- `POST /update` - update note
- `POST /delete` - delete note
- `POST /search` - standard RAG response
- `POST /search/stream` - SSE streaming response
- `POST /history` - list note snapshots
- `POST /restore-version` - restore a previous snapshot

## Frontend

The frontend is a Vite React app using `.jsx` files only for application code.

Install and run from `frontend/`:

```bash
npm install
npm run dev
```

If the backend is running on a different host or port, set `VITE_API_BASE_URL` before starting Vite.

## One Command Start

From the repository root, run:

```powershell
.\start-dev.ps1
```

It will build the backend executable if needed and start backend and frontend in separate PowerShell windows.

## Notes

- Add, edit, and delete notes from the Notes view.
- Open note history and restore older versions from the Notes view.
- Ask questions in the Chat view; answers are grounded in stored notes.
- Chat supports backend-driven token streaming for a typing effect.
- Flashcards, graph relationships, contradictions, ideas, and learning path data are all derived from the stored notes.

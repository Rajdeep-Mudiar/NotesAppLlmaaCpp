import React, { useEffect, useMemo, useState } from "react";
import './styles/app.css';
import './styles/chat.css';
import Sidebar from "./components/Sidebar.jsx";
import Chat from "./components/Chat.jsx";
import Notes from "./components/Notes.jsx";
import Flashcards from "./components/Flashcards.jsx";
import Quiz from "./components/Quiz.jsx";
import Graph from "./components/Graph.jsx";
import Roadmap from "./components/Roadmap.jsx";
import Summarizer from "./components/Summarizer.jsx";

const API_BASE = import.meta.env.VITE_API_BASE_URL || "http://127.0.0.1:8080";

const views = [
  { id: "chat", label: "Chat" },
  { id: "notes", label: "Notes" },
  { id: "flashcards", label: "Flashcards" },
  { id: "quiz", label: "Quiz Arena" },
  { id: "roadmap", label: "Roadmap" },
  { id: "summarizer", label: "Summarizer" },
];

async function request(path, payload) {
  const response = await fetch(`${API_BASE}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: payload ? JSON.stringify(payload) : "{}",
  });
  if (!response.ok) {
    throw new Error(`Request failed: ${response.status}`);
  }
  return response.json();
}

async function loadNotes() {
  const response = await fetch(`${API_BASE}/notes`);
  if (!response.ok) {
    throw new Error(`Failed to load notes: ${response.status}`);
  }
  const data = await response.json();
  return data.notes || [];
}

export default function App() {
  const [activeView, setActiveView] = useState("chat");
  const [notes, setNotes] = useState([]);
  const [insights, setInsights] = useState({});
  const [flashcards, setFlashcards] = useState([]);
  const [quizQuestions, setQuizQuestions] = useState([]);
  const [graph, setGraph] = useState({ nodes: [], edges: [] });
  const [contradictions, setContradictions] = useState([]);
  const [learningPath, setLearningPath] = useState([]);
  const [roadmapData, setRoadmapData] = useState([]);
  const [flashcardsHistory, setFlashcardsHistory] = useState([]);
  const [quizHistory, setQuizHistory] = useState([]);
  const [roadmapHistory, setRoadmapHistory] = useState([]);
  const [summaryHistory, setSummaryHistory] = useState([]);
  const [selfQuestions, setSelfQuestions] = useState([]);
  const [ideas, setIdeas] = useState([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  const [messages, setMessages] = useState([]);

  async function handleChat(query, mode, persona) {
    setBusy(true);
    setError("");
    const userMsg = { role: "user", text: query };
    setMessages((prev) => [...prev, userMsg]);

    try {
      const data = await request("/search", { query, mode, persona });
      const assistantMsg = { role: "assistant", text: data.answer || "No response." };
      setMessages((prev) => [...prev, assistantMsg]);
    } catch (e) {
      setError("Chat error: " + e.message);
    } finally {
      setBusy(false);
    }
  }

  async function handleSaveNote(note) {
    setBusy(true);
    try {
      await request("/save-note", note);
      await refreshAll();
    } catch (e) {
      setError("Save failed: " + e.message);
    } finally {
      setBusy(false);
    }
  }

  async function handleDeleteNote(id) {
    if (!window.confirm("Are you sure?")) return;
    setBusy(true);
    try {
      await request("/delete-note", { id });
      await refreshAll();
    } catch (e) {
      setError("Delete failed: " + e.message);
    } finally {
      setBusy(false);
    }
  }

  async function handleGenerateRoadmap(noteIds) {
    setBusy(true);
    setError("");
    try {
      const data = await request("/roadmap", { noteIds });
      setRoadmapData(data.roadmap || []);
    } catch (e) {
      setError("Roadmap failed: " + e.message);
    } finally {
      setBusy(false);
    }
  }

  const stats = useMemo(
    () => ({
      notes: notes.length,
      tags: new Set(notes.flatMap((note) => note.tags || [])).size,
      contradictions: contradictions.length,
      flashcards: flashcards.length,
    }),
    [notes, contradictions, flashcards],
  );

  async function refreshAll() {
    setBusy(true);
    setError("");
    try {
      const [
        latestNotes,
        flashcardsData,
        graphData,
        contradictionsData,
        fhRes,
        qhRes,
        rhRes,
        shRes
      ] = await Promise.all([
        loadNotes(),
        request("/flashcards"),
        request("/graph"),
        request("/contradictions"),
        fetch(`${API_BASE}/history/flashcards`).then(r => r.json()),
        fetch(`${API_BASE}/history/quizzes`).then(r => r.json()),
        fetch(`${API_BASE}/history/roadmaps`).then(r => r.json()),
        fetch(`${API_BASE}/history/summaries`).then(r => r.json())
      ]);

      setNotes(latestNotes);
      setFlashcards(flashcardsData.flashcards || []);
      setGraph(graphData);
      setContradictions(contradictionsData.contradictions || []);
      setFlashcardsHistory(fhRes.history || []);
      setQuizHistory(qhRes.history || []);
      setRoadmapHistory(rhRes.history || []);
      setSummaryHistory(shRes.history || []);
    } catch (requestError) {
      setError(requestError.message || "Unable to sync with the backend");
    } finally {
      setBusy(false);
    }
  }

  useEffect(() => {
    refreshAll();
  }, []);

  return (
    <div className="app-shell">
      <Sidebar
        views={views}
        activeView={activeView}
        onSwitch={setActiveView}
      />

      <main className="main-panel">
        <header className="header-bar" style={{ marginBottom: "8px" }}>
          <div className="hero-stats" style={{ background: "transparent", border: "none", boxShadow: "none", padding: 0 }}>
            <div style={{ marginRight: "24px" }}>
              <span style={{ fontSize: "1.5rem", fontWeight: "700" }}>{stats.notes}</span>
              <label style={{ marginLeft: "8px", color: "var(--text-muted)", fontSize: "0.8rem" }}>Notes</label>
            </div>
            <div style={{ marginRight: "24px" }}>
              <span style={{ fontSize: "1.5rem", fontWeight: "700" }}>{stats.tags}</span>
              <label style={{ marginLeft: "8px", color: "var(--text-muted)", fontSize: "0.8rem" }}>Tags</label>
            </div>
          </div>
          {busy && <div className="tag" style={{ background: "var(--accent)", color: "white" }}>AI Active</div>}
        </header>

        {error ? <div className="error-banner" style={{ margin: "0 0 16px 0", background: "var(--danger)", color: "white", padding: "12px", borderRadius: "var(--radius-md)", display: "flex", justifyContent: "space-between" }}>
          <span>{error}</span>
          <button onClick={() => setError("")} style={{ background: "transparent", border: "none", color: "white", cursor: "pointer", fontWeight: "bold" }}>×</button>
        </div> : null}

        <div className="view-content">
          {activeView === "chat" ? (
            <Chat
              messages={messages}
              onSend={handleChat}
              busy={busy}
            />
          ) : null}

          {activeView === "notes" ? (
            <Notes 
              notes={notes} 
              onSave={handleSaveNote} 
              onDelete={handleDeleteNote} 
            />
          ) : null}

          {activeView === "flashcards" ? (
            <Flashcards
              flashcards={flashcards}
              history={flashcardsHistory}
              notes={notes}
              onLoadHistory={(item) => setFlashcards(item.flashcards || [])}
              onGenerateFlashcards={async (count, difficulty, noteIds) => {
                setBusy(true);
                try {
                  const data = await request("/flashcards", { count, difficulty, noteIds });
                  setFlashcards(data.flashcards || []);
                  refreshAll(); // Refresh history
                } catch (e) {
                  setError("Failed to generate flashcards: " + e.message);
                } finally {
                  setBusy(false);
                }
              }}
              busy={busy}
            />
          ) : null}

          {activeView === "quiz" ? (
            <Quiz
              quiz={quizQuestions}
              history={quizHistory}
              notes={notes}
              onLoadHistory={(item) => setQuizQuestions(item.questions || [])}
              onGenerateQuiz={async (count, difficulty, noteIds) => {
                setBusy(true);
                try {
                  const data = await request("/quiz", { count, difficulty, noteIds });
                  setQuizQuestions(data.questions || []);
                  refreshAll(); // Refresh history
                } catch (e) {
                  setError("Failed to generate quiz: " + e.message);
                } finally {
                  setBusy(false);
                }
              }}
              busy={busy}
            />
          ) : null}



          {activeView === "roadmap" ? (
            <Roadmap 
              roadmap={roadmapData} 
              history={roadmapHistory}
              notes={notes} 
              onLoadHistory={(item) => setRoadmapData(item.roadmap || [])}
              onGenerateRoadmap={async (noteIds) => {
                await handleGenerateRoadmap(noteIds);
                refreshAll(); // Refresh history
              }}
              busy={busy}
            />
          ) : null}

          {activeView === "summarizer" ? (
            <Summarizer
              notes={notes}
              history={summaryHistory}
              busy={busy}
              onSummarize={async (noteIds) => {
                setBusy(true);
                try {
                  const data = await request("/summarize", { noteIds });
                  refreshAll(); // Refresh history
                  return data;
                } catch (e) {
                  setError("Failed to generate summary: " + e.message);
                  return null;
                } finally {
                  setBusy(false);
                }
              }}
            />
          ) : null}
        </div>
      </main>
    </div>
  );
}

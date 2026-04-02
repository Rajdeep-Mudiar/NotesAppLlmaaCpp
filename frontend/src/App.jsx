import { useEffect, useMemo, useState } from "react";
import Sidebar from "./components/Sidebar.jsx";
import Chat from "./components/Chat.jsx";
import Notes from "./components/Notes.jsx";
import Flashcards from "./components/Flashcards.jsx";
import Graph from "./components/Graph.jsx";

const API_BASE = import.meta.env.VITE_API_BASE_URL || "http://127.0.0.1:8080";

const views = [
  { id: "chat", label: "Chat" },
  { id: "notes", label: "Notes" },
  { id: "flashcards", label: "Flashcards" },
  { id: "graph", label: "Knowledge Graph" },
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
  const [graph, setGraph] = useState({ nodes: [], edges: [] });
  const [contradictions, setContradictions] = useState([]);
  const [learningPath, setLearningPath] = useState([]);
  const [selfQuestions, setSelfQuestions] = useState([]);
  const [ideas, setIdeas] = useState([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

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
        insightsData,
        flashcardsData,
        graphData,
        contradictionsData,
        learningPathData,
        questionsData,
        ideasData,
      ] = await Promise.all([
        loadNotes(),
        request("/insights"),
        request("/flashcards"),
        request("/graph"),
        request("/contradictions"),
        request("/learning-path"),
        request("/questions"),
        request("/ideas"),
      ]);

      setNotes(latestNotes);
      setInsights(insightsData);
      setFlashcards(flashcardsData.flashcards || []);
      setGraph(graphData);
      setContradictions(contradictionsData.contradictions || []);
      setLearningPath(learningPathData.learning_path || []);
      setSelfQuestions(questionsData.self_questions || []);
      setIdeas(ideasData.ideas || []);
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
      <div className="ambient ambient-one" />
      <div className="ambient ambient-two" />
      <Sidebar
        activeView={activeView}
        setActiveView={setActiveView}
        views={views}
        stats={stats}
        onRefresh={refreshAll}
        busy={busy}
      />

      <main className="main-panel">
        <section className="hero-card">
          <div>
            <p className="eyebrow">AI Second Brain Notes Application</p>
            <h1>Store notes, ask questions, and learn from local AI.</h1>
            <p className="hero-copy">
              A privacy-focused workspace for notes, retrieval, flashcards,
              contradictions, and knowledge graphs powered by llama.cpp.
            </p>
          </div>
          <div className="hero-stats">
            <div>
              <span>{stats.notes}</span>
              <label>Notes</label>
            </div>
            <div>
              <span>{stats.tags}</span>
              <label>Tags</label>
            </div>
            <div>
              <span>{stats.contradictions}</span>
              <label>Conflicts</label>
            </div>
          </div>
        </section>

        {error ? <div className="error-banner">{error}</div> : null}

        {activeView === "chat" ? (
          <Chat
            notes={notes}
            insights={insights}
            onSync={refreshAll}
            apiBase={API_BASE}
          />
        ) : null}

        {activeView === "notes" ? (
          <Notes notes={notes} onChanged={refreshAll} apiBase={API_BASE} />
        ) : null}

        {activeView === "flashcards" ? (
          <Flashcards
            flashcards={flashcards}
            ideas={ideas}
            learningPath={learningPath}
            selfQuestions={selfQuestions}
          />
        ) : null}

        {activeView === "graph" ? (
          <Graph graph={graph} contradictions={contradictions} />
        ) : null}
      </main>
    </div>
  );
}

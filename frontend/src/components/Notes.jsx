import { useEffect, useMemo, useState } from "react";

function parseTags(value) {
  return value
    .split(",")
    .map((tag) => tag.trim().toLowerCase())
    .filter(Boolean);
}

async function apiRequest(apiBase, path, payload) {
  const response = await fetch(`${apiBase}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });
  if (!response.ok) {
    throw new Error(`Request failed: ${response.status}`);
  }
  return response.json();
}

export default function Notes({ notes, onChanged, apiBase }) {
  const [title, setTitle] = useState("");
  const [content, setContent] = useState("");
  const [tags, setTags] = useState("");
  const [editingId, setEditingId] = useState("");
  const [filter, setFilter] = useState("");
  const [busy, setBusy] = useState(false);
  const [historyForId, setHistoryForId] = useState("");
  const [historyVersions, setHistoryVersions] = useState([]);
  const [historyBusy, setHistoryBusy] = useState(false);

  useEffect(() => {
    if (!editingId) {
      return;
    }
    const note = notes.find((entry) => entry.id === editingId);
    if (note) {
      setTitle(note.title || "");
      setContent(note.content || "");
      setTags((note.tags || []).join(", "));
    }
  }, [editingId, notes]);

  const visibleNotes = useMemo(() => {
    const lowered = filter.toLowerCase();
    if (!lowered) {
      return notes;
    }
    return notes.filter((note) => {
      const joinedTags = (note.tags || []).join(" ");
      return `${note.title} ${note.content} ${joinedTags}`
        .toLowerCase()
        .includes(lowered);
    });
  }, [notes, filter]);

  async function saveNote() {
    if (!content.trim()) {
      return;
    }

    setBusy(true);
    try {
      if (editingId) {
        await apiRequest(apiBase, "/update", {
          id: editingId,
          title,
          content,
          tags: parseTags(tags),
        });
      } else {
        await apiRequest(apiBase, "/add", {
          title,
          content,
          tags: parseTags(tags),
        });
      }
      setTitle("");
      setContent("");
      setTags("");
      setEditingId("");
      onChanged();
    } finally {
      setBusy(false);
    }
  }

  async function removeNote(id) {
    setBusy(true);
    try {
      await apiRequest(apiBase, "/delete", { id });
      onChanged();
    } finally {
      setBusy(false);
    }
  }

  async function openHistory(id) {
    setHistoryForId(id);
    setHistoryBusy(true);
    try {
      const data = await apiRequest(apiBase, "/history", { id });
      setHistoryVersions(data.versions || []);
    } finally {
      setHistoryBusy(false);
    }
  }

  async function restoreVersion(id, versionFile) {
    setBusy(true);
    try {
      await apiRequest(apiBase, "/restore-version", {
        id,
        version_file: versionFile,
      });
      await openHistory(id);
      await onChanged();
    } finally {
      setBusy(false);
    }
  }

  return (
    <section className="panel-grid notes-layout">
      <div className="panel form-panel">
        <div className="panel-header">
          <div>
            <p className="panel-kicker">Notes system</p>
            <h3>{editingId ? "Edit note" : "Create a note"}</h3>
          </div>
          <input
            className="search-input"
            value={filter}
            onChange={(event) => setFilter(event.target.value)}
            placeholder="Filter notes"
          />
        </div>

        <div className="form-grid">
          <label>
            Title
            <input
              value={title}
              onChange={(event) => setTitle(event.target.value)}
              placeholder="A concise title"
            />
          </label>
          <label>
            Tags
            <input
              value={tags}
              onChange={(event) => setTags(event.target.value)}
              placeholder="ai, learning, exam"
            />
          </label>
        </div>
        <label>
          Content
          <textarea
            value={content}
            onChange={(event) => setContent(event.target.value)}
            rows={8}
            placeholder="Write your note here"
          />
        </label>

        <div className="composer-actions">
          <button
            type="button"
            className="primary-button"
            onClick={saveNote}
            disabled={busy}
          >
            {editingId ? "Update note" : "Add note"}
          </button>
          {editingId ? (
            <button
              type="button"
              className="ghost-button"
              onClick={() => setEditingId("")}
            >
              Cancel edit
            </button>
          ) : null}
        </div>
      </div>

      <div className="note-grid">
        {visibleNotes.map((note) => (
          <article className="note-card panel" key={note.id}>
            <div className="note-card-head">
              <div>
                <h4>{note.title}</h4>
                <p>{note.updated_at}</p>
              </div>
              <span className="card-badge">
                {(note.tags || []).length} tags
              </span>
            </div>
            <p className="note-summary">{note.content}</p>
            <div className="pill-cloud">
              {(note.tags || []).map((tag) => (
                <span key={tag} className="pill">
                  {tag}
                </span>
              ))}
            </div>
            <div className="card-actions">
              <button
                type="button"
                className="ghost-button"
                onClick={() => setEditingId(note.id)}
              >
                Edit
              </button>
              <button
                type="button"
                className="ghost-button"
                onClick={() => openHistory(note.id)}
              >
                History
              </button>
              <button
                type="button"
                className="ghost-button danger"
                onClick={() => removeNote(note.id)}
              >
                Delete
              </button>
            </div>
          </article>
        ))}
      </div>

      {historyForId ? (
        <div className="panel history-panel">
          <div className="panel-header">
            <div>
              <p className="panel-kicker">Version history</p>
              <h3>Snapshots for {historyForId}</h3>
            </div>
            <button
              type="button"
              className="ghost-button"
              onClick={() => {
                setHistoryForId("");
                setHistoryVersions([]);
              }}
            >
              Close
            </button>
          </div>

          {historyBusy ? <p>Loading history...</p> : null}
          {!historyBusy && historyVersions.length === 0 ? (
            <p>No snapshots found for this note.</p>
          ) : null}

          {!historyBusy ? (
            <ul className="compact-list">
              {historyVersions.map((version) => (
                <li key={version.file}>
                  <strong>{version.title || "Untitled"}</strong>
                  <span>
                    {version.timestamp} · {version.reason}
                  </span>
                  <div className="card-actions">
                    <button
                      type="button"
                      className="ghost-button"
                      onClick={() => restoreVersion(historyForId, version.file)}
                      disabled={busy}
                    >
                      Restore this version
                    </button>
                  </div>
                </li>
              ))}
            </ul>
          ) : null}
        </div>
      ) : null}
    </section>
  );
}

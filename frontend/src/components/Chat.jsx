import { useEffect, useMemo, useRef, useState } from "react";

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

async function streamSearch(apiBase, payload, onToken) {
  const response = await fetch(`${apiBase}/search/stream`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });

  if (!response.ok || !response.body) {
    throw new Error(`Stream request failed: ${response.status}`);
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder("utf-8");
  let buffer = "";
  let finalResponse = null;

  while (true) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }

    buffer += decoder.decode(value, { stream: true });

    let boundary = buffer.indexOf("\n\n");
    while (boundary !== -1) {
      const eventBlock = buffer.slice(0, boundary);
      buffer = buffer.slice(boundary + 2);

      const dataLine = eventBlock
        .split("\n")
        .find((line) => line.startsWith("data: "));

      if (dataLine) {
        const payloadText = dataLine.slice(6).trim();
        if (payloadText) {
          const eventData = JSON.parse(payloadText);
          if (eventData.type === "token") {
            onToken(eventData.content || "");
          }
          if (eventData.type === "done") {
            finalResponse = eventData.response || null;
          }
          if (eventData.type === "error") {
            throw new Error(eventData.message || "Streaming failed");
          }
        }
      }

      boundary = buffer.indexOf("\n\n");
    }
  }

  return finalResponse;
}

export default function Chat({ notes, insights, onSync, apiBase }) {
  const [messages, setMessages] = useState([
    {
      role: "assistant",
      text: "Ask a question about your notes. I will answer from the stored context only.",
    },
  ]);
  const [input, setInput] = useState("");
  const [mode, setMode] = useState("chat");
  const [persona, setPersona] = useState("teacher");
  const [sending, setSending] = useState(false);
  const bottomRef = useRef(null);

  const notePreview = useMemo(() => notes.slice(0, 3), [notes]);

  useEffect(() => {
    bottomRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [messages]);

  async function sendMessage() {
    const question = input.trim();
    if (!question || sending) {
      return;
    }

    setSending(true);
    setInput("");

    let assistantIndex = -1;
    setMessages((current) => {
      assistantIndex = current.length + 1;
      return [
        ...current,
        { role: "user", text: question },
        { role: "assistant", text: "" },
      ];
    });

    const payload = { query: question, mode, persona };

    try {
      const streamedResponse = await streamSearch(apiBase, payload, (chunk) => {
        setMessages((current) => {
          if (assistantIndex < 0 || !current[assistantIndex]) {
            return current;
          }
          const next = [...current];
          next[assistantIndex] = {
            ...next[assistantIndex],
            text: `${next[assistantIndex].text}${chunk}`,
          };
          return next;
        });
      });

      if (!streamedResponse) {
        throw new Error("No streamed completion payload received");
      }

      await onSync();
    } catch (streamError) {
      try {
        const fallback = await apiRequest(apiBase, "/search", payload);
        setMessages((current) => {
          if (assistantIndex < 0 || !current[assistantIndex]) {
            return current;
          }
          const next = [...current];
          next[assistantIndex] = {
            ...next[assistantIndex],
            text: fallback.answer || "No answer returned by the backend.",
          };
          return next;
        });
        await onSync();
      } catch (fallbackError) {
        setMessages((current) => {
          if (assistantIndex < 0 || !current[assistantIndex]) {
            return current;
          }
          const next = [...current];
          next[assistantIndex] = {
            ...next[assistantIndex],
            text:
              fallbackError.message ||
              streamError.message ||
              "Failed to ask the backend.",
          };
          return next;
        });
      }
    } finally {
      setSending(false);
    }
  }

  return (
    <section className="panel-grid chat-layout">
      <div className="panel chat-panel">
        <div className="panel-header">
          <div>
            <p className="panel-kicker">ChatGPT-style retrieval</p>
            <h3>Ask questions grounded in your notes</h3>
          </div>
          <div className="mode-chip-row">
            <select
              value={mode}
              onChange={(event) => setMode(event.target.value)}
            >
              <option value="chat">Chat</option>
              <option value="argument">Argument</option>
              <option value="idea">Idea generation</option>
            </select>
            <select
              value={persona}
              onChange={(event) => setPersona(event.target.value)}
            >
              <option value="teacher">Teacher</option>
              <option value="critic">Critic</option>
              <option value="examiner">Examiner</option>
            </select>
          </div>
        </div>

        <div className="chat-stream">
          {messages.map((message, index) => (
            <div
              key={`${message.role}-${index}`}
              className={
                message.role === "user" ? "message user" : "message assistant"
              }
            >
              <span className="message-role">{message.role}</span>
              <p>
                {message.text ||
                  (sending && index === messages.length - 1
                    ? "Thinking..."
                    : "")}
              </p>
            </div>
          ))}
          <div ref={bottomRef} />
        </div>

        <div className="composer">
          <textarea
            placeholder="Ask about a concept, compare notes, or request an explanation..."
            value={input}
            onChange={(event) => setInput(event.target.value)}
            rows={3}
          />
          <div className="composer-actions">
            <button
              type="button"
              className="primary-button"
              onClick={sendMessage}
              disabled={sending}
            >
              {sending ? "Streaming..." : "Send"}
            </button>
          </div>
        </div>
      </div>

      <aside className="side-panel">
        <div className="panel compact-panel">
          <p className="panel-kicker">Knowledge snapshot</p>
          <h4>{notes.length} stored notes</h4>
          <ul className="compact-list">
            {notePreview.map((note) => (
              <li key={note.id}>
                <strong>{note.title}</strong>
                <span>{(note.tags || []).join(" · ") || "untagged"}</span>
              </li>
            ))}
          </ul>
        </div>

        <div className="panel compact-panel">
          <p className="panel-kicker">Retrieval cues</p>
          <div className="pill-cloud">
            {(insights.recent_notes || [])
              .flatMap((note) => note.tags || [])
              .slice(0, 8)
              .map((tag) => (
                <span key={tag} className="pill">
                  {tag}
                </span>
              ))}
          </div>
        </div>
      </aside>
    </section>
  );
}

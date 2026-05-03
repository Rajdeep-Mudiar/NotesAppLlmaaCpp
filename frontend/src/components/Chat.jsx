import React, { useEffect, useMemo, useRef, useState } from "react";

// How long (ms) to wait for a streamed LLM response before giving up.
// Set to 3 minutes — llama-cli on CPU can take 1-2 min for 256 tokens.
const STREAM_TIMEOUT_MS = 3 * 60 * 1000;

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

async function streamSearch(apiBase, payload, onToken, signal) {
  const response = await fetch(`${apiBase}/search/stream`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
    signal,
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
  const [statusText, setStatusText] = useState("Thinking...");
  const bottomRef = useRef(null);
  // Tracks the index of the in-progress assistant message in a ref so it is
  // always fresh inside async callbacks without stale-closure issues.
  const assistantIndexRef = useRef(-1);

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
    setStatusText("Thinking...");
    setInput("");

    // Compute the future index of the assistant placeholder BEFORE setState
    // so there is no stale-closure / race condition.
    setMessages((current) => {
      assistantIndexRef.current = current.length + 1; // +1 because user msg is inserted first
      return [
        ...current,
        { role: "user", text: question },
        { role: "assistant", text: "" },
      ];
    });

    const payload = { query: question, mode, persona };

    // AbortController lets us cancel after STREAM_TIMEOUT_MS
    const abortController = new AbortController();
    const timeoutId = setTimeout(() => {
      abortController.abort();
      setStatusText("The AI is taking too long (still loading model). Please wait and retry.");
    }, STREAM_TIMEOUT_MS);

    const updateAssistant = (text) => {
      setMessages((current) => {
        const idx = assistantIndexRef.current;
        if (idx < 0 || idx >= current.length) return current;
        const next = [...current];
        next[idx] = { ...next[idx], text };
        return next;
      });
    };

    try {
      let accumulated = "";
      let firstToken = true;
      setStatusText("Loading model... (this takes ~30–60 s on first run)");

      const streamedResponse = await streamSearch(
        apiBase,
        payload,
        (chunk) => {
          if (firstToken) {
            setStatusText("Receiving response...");
            firstToken = false;
          }
          accumulated += chunk;
          updateAssistant(accumulated);
        },
        abortController.signal
      );

      clearTimeout(timeoutId);

      if (!streamedResponse) {
        throw new Error("Stream ended without a completion payload.");
      }

      // If streaming delivered tokens, keep them; otherwise use the done payload.
      if (!accumulated && streamedResponse.answer) {
        updateAssistant(streamedResponse.answer);
      }

      await onSync();
    } catch (streamError) {
      clearTimeout(timeoutId);

      // Don't fall back if the user explicitly aborted (timeout)
      if (streamError.name === "AbortError") {
        updateAssistant(
          "⏱ The request timed out (3 minutes). The model is very slow on CPU-only mode. " +
          "Try asking a shorter question or wait for the model to finish loading."
        );
        setSending(false);
        return;
      }

      // SSE stream failed — fall back to regular POST /search
      setStatusText("Stream failed, trying direct request...");
      try {
        const fallback = await apiRequest(apiBase, "/search", payload);
        updateAssistant(
          fallback.answer || "The AI returned an empty response. Check the backend console."
        );
        await onSync();
      } catch (fallbackError) {
        updateAssistant(
          `Error: ${fallbackError.message || streamError.message || "Backend request failed."}` +
          " Make sure the backend server is running on port 8080."
        );
      }
    } finally {
      clearTimeout(timeoutId);
      setSending(false);
      setStatusText("Thinking...");
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
                    ? statusText
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

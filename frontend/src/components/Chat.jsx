import React, { useState, useEffect, useRef } from "react";

export default function Chat({ messages, onSend, busy }) {
  const [input, setInput] = useState("");
  const [mode, setMode] = useState("chat");
  const [persona, setPersona] = useState("teacher");
  const scrollRef = useRef(null);

  // Auto-scroll to bottom on new messages
  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages, busy]);

  const handleSubmit = (e) => {
    e.preventDefault();
    if (!input.trim() || busy) return;
    onSend(input, mode, persona);
    setInput("");
  };

  return (
    <div className="chat-view">
      <div className="view-header" style={{ marginBottom: '1.5rem' }}>
        <div className="view-title">
          <h2>Second Brain Chat</h2>
          <p>Querying {messages.length} interactions in current session</p>
        </div>
        <div className="notes-controls">
          <select className="search-bar" style={{width: 'auto'}} value={mode} onChange={(e) => setMode(e.target.value)}>
            <option value="chat">Chat Mode</option>
            <option value="summarize">Summarize</option>
            <option value="critique">Critique</option>
          </select>
          <select className="search-bar" style={{width: 'auto'}} value={persona} onChange={(e) => setPersona(e.target.value)}>
            <option value="teacher">Teacher Persona</option>
            <option value="student">Student Persona</option>
            <option value="expert">Expert Persona</option>
            <option value="critic">Reviewer Persona</option>
          </select>
        </div>
      </div>

      <div className="messages-container" ref={scrollRef}>
        {messages.length === 0 && (
          <div className="view-title" style={{ textAlign: 'center', marginTop: '4rem', opacity: 0.5 }}>
            <div style={{ fontSize: '3rem', marginBottom: '1rem' }}>🧠</div>
            <p>Ask me anything about your notes. I'll search through everything you've saved.</p>
          </div>
        )}
        
        {messages.map((msg, i) => (
          <div key={i} className={`message ${msg.role}`}>
            <div className="message-header" style={{fontSize: '0.7rem', opacity: 0.6, marginBottom: '0.25rem', display: 'flex', justifyContent: msg.role === 'user' ? 'flex-end' : 'flex-start'}}>
              {msg.role === 'user' ? 'You' : 'Assistant'}
            </div>
            <div className="message-text">
              {msg.text}
            </div>
          </div>
        ))}

        {busy && (
          <div className="message assistant">
            <div className="message-header" style={{fontSize: '0.7rem', opacity: 0.6, marginBottom: '0.25rem'}}>
              Assistant
            </div>
            <div className="loading-dots">Consulting your knowledge base</div>
          </div>
        )}
      </div>

      <form className="chat-input-wrapper" onSubmit={handleSubmit} style={{ marginTop: 'auto' }}>
        <input
          type="text"
          className="chat-input"
          placeholder="Type your question here..."
          value={input}
          onChange={(e) => setInput(e.target.value)}
          disabled={busy}
        />
        <button type="submit" className="btn-send" disabled={busy || !input.trim()}>
          <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
            <line x1="22" y1="2" x2="11" y2="13"></line>
            <polygon points="22 2 15 22 11 13 2 9 22 2"></polygon>
          </svg>
        </button>
      </form>
    </div>
  );
}

import React, { useState } from "react";
import "../styles/flashcards.css";

export default function Flashcards({ flashcards, history, notes, onLoadHistory, onGenerateFlashcards, busy }) {
  const [activeIndex, setActiveIndex] = useState(0);
  const [flipped, setFlipped] = useState(false);
  const [configuring, setConfiguring] = useState(true);

  // Form states
  const [count, setCount] = useState(5);
  const [difficulty, setDifficulty] = useState("medium");
  const [selectedNoteIds, setSelectedNoteIds] = useState([]);

  const activeCard = flashcards[activeIndex] || null;
  const progress = flashcards.length > 0 ? ((activeIndex + 1) / flashcards.length) * 100 : 0;

  const handleToggleNote = (id) => {
    setSelectedNoteIds(prev => 
      prev.includes(id) ? prev.filter(i => i !== id) : [...prev, id]
    );
  };

  const handleGenerate = async () => {
    if (busy) return;
    await onGenerateFlashcards(count, difficulty, selectedNoteIds);
    setActiveIndex(0);
    setFlipped(false);
    setConfiguring(false);
  };

  if (configuring) {
    return (
      <div className="flashcards-view" style={{ padding: '2rem', display: 'flex', gap: '2rem', alignItems: 'flex-start' }}>
        <div className="setup-container" style={{ flex: 2 }}>
          <header style={{ marginBottom: '2.5rem', textAlign: 'center' }}>
            <h1 style={{ fontSize: '2rem', fontWeight: '800', color: '#0f172a', marginBottom: '0.5rem' }}>Study Lab</h1>
            <p style={{ color: '#64748b' }}>Configure your active recall session</p>
          </header>

          <div className="setup-step">
            <label><span className="step-num">1</span> How many cards?</label>
            <div style={{ display: 'flex', alignItems: 'center', gap: '15px' }}>
                <input 
                    type="range" min="3" max="20" step="1"
                    style={{ flex: 1, accentColor: 'var(--accent)' }}
                    value={count}
                    onChange={e => setCount(parseInt(e.target.value))}
                />
                <span style={{ fontWeight: '800', fontSize: '1.2rem', color: 'var(--accent)', minWidth: '30px' }}>{count}</span>
            </div>
          </div>

          <div className="setup-step">
            <label><span className="step-num">2</span> Select Difficulty</label>
            <div className="difficulty-picker">
              {['easy', 'medium', 'hard'].map(d => (
                <button 
                  key={d}
                  className={`difficulty-btn ${difficulty === d ? 'active' : ''}`}
                  onClick={() => setDifficulty(d)}
                >
                  {d}
                </button>
              ))}
            </div>
          </div>

          <div className="setup-step">
            <label><span className="step-num">3</span> Source Notes ({selectedNoteIds.length})</label>
            <div className="notes-selector">
              {notes.map(note => (
                <div 
                  key={note.id} 
                  className={`note-option ${selectedNoteIds.includes(note.id) ? 'selected' : ''}`}
                  onClick={() => handleToggleNote(note.id)}
                >
                  <div style={{ 
                      width: '20px', height: '20px', borderRadius: '5px', 
                      border: '2px solid #e2e8f0', display: 'flex', alignItems: 'center', 
                      justifyContent: 'center', background: selectedNoteIds.includes(note.id) ? 'var(--accent)' : 'white',
                      borderColor: selectedNoteIds.includes(note.id) ? 'var(--accent)' : '#e2e8f0'
                  }}>
                      {selectedNoteIds.includes(note.id) && <span style={{ color: 'white', fontSize: '12px' }}>✓</span>}
                  </div>
                  <span style={{ fontWeight: '600' }}>{note.title}</span>
                </div>
              ))}
              {notes.length === 0 && <p style={{ padding: '20px', textAlign: 'center', color: '#94a3b8' }}>No notes available to generate from.</p>}
            </div>
          </div>

          <button 
            className="btn-add" 
            style={{ width: '100%', height: '60px', borderRadius: '18px', fontSize: '1.1rem', fontWeight: '700', marginTop: '1rem' }}
            disabled={busy || notes.length === 0}
            onClick={handleGenerate}
          >
            {busy ? "🧠 Synthesizing Deck..." : "Launch Session"}
          </button>
        </div>

        <div className="setup-container" style={{ flex: 1, background: '#f8fafc', border: '1px dashed #cbd5e1' }}>
          <h3 style={{ marginBottom: '1.5rem', fontWeight: '800', color: '#334155' }}>Recent History</h3>
          {history && history.length > 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              {history.slice().reverse().map(item => (
                <div 
                  key={item.id} 
                  className="history-card"
                  style={{ padding: '15px', background: 'white', borderRadius: '12px', border: '1px solid #e2e8f0', cursor: 'pointer', transition: 'all 0.2s' }}
                  onClick={() => { onLoadHistory(item); setActiveIndex(0); setFlipped(false); setConfiguring(false); }}
                  onMouseEnter={(e) => { e.currentTarget.style.borderColor = 'var(--accent)'; e.currentTarget.style.transform = 'translateY(-2px)'; }}
                  onMouseLeave={(e) => { e.currentTarget.style.borderColor = '#e2e8f0'; e.currentTarget.style.transform = 'translateY(0)'; }}
                >
                  <div style={{ fontWeight: '700', color: '#0f172a' }}>{item.flashcards?.length || 0} Cards Generated</div>
                  <div style={{ fontSize: '0.8rem', color: '#64748b', marginTop: '4px' }}>
                    {new Date(item.created_at).toLocaleString()}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <p style={{ color: '#94a3b8', fontSize: '0.9rem', textAlign: 'center' }}>No recent flashcard sessions.</p>
          )}
        </div>
      </div>
    );
  }

  return (
    <div className="flashcards-view">
      <div className="header-bar" style={{ padding: '1.5rem 2rem', background: 'white', borderBottom: '1px solid #f1f5f9' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{ background: 'var(--accent)', color: 'white', padding: '8px', borderRadius: '10px' }}>📚</div>
            <h3 style={{ fontWeight: '800' }}>Study Session</h3>
        </div>
        <button className="btn-add" style={{ background: '#f1f5f9', color: '#475569', fontSize: '0.9rem' }} onClick={() => setConfiguring(true)}>
          Exit Session
        </button>
      </div>

      <div className="card-scene">
        {activeCard && (
           <div className="progress-container">
             <div className="progress-bar" style={{ width: `${progress}%` }}></div>
           </div>
        )}

        {activeCard ? (
          <div className={`study-card ${flipped ? 'flipped' : ''}`} onClick={() => setFlipped(!flipped)}>
            <div className="card-inner">
              <div className="card-face card-front">
                <span className="card-tag" style={{ color: 'var(--accent)' }}>QUESTION</span>
                <div className="card-content">{activeCard.front}</div>
                <div style={{ marginTop: 'auto', color: '#94a3b8', fontSize: '0.9rem', fontWeight: '600' }}>Click to Flip</div>
              </div>
              <div className="card-face card-back">
                <span className="card-tag">ANSWER</span>
                <div className="card-content">{activeCard.back}</div>
                <div style={{ marginTop: 'auto', color: 'rgba(255,255,255,0.7)', fontSize: '0.9rem', fontWeight: '600' }}>Click to Hide</div>
              </div>
            </div>
          </div>
        ) : (
          <div className="setup-container" style={{ textAlign: 'center', padding: '4rem 2rem' }}>
            <div style={{ fontSize: '4rem', marginBottom: '1.5rem' }}>🎯</div>
            <h2 style={{ fontWeight: '800', marginBottom: '1rem' }}>Session Complete!</h2>
            <p style={{ color: '#64748b', marginBottom: '2rem' }}>Great job with your active recall. Ready for another round?</p>
            <button className="btn-add" style={{ padding: '1rem 3rem' }} onClick={() => setConfiguring(true)}>Start New Deck</button>
          </div>
        )}
      </div>

      {flashcards.length > 0 && activeCard && (
        <div className="study-nav">
          <button
            className="nav-circle-btn"
            onClick={(e) => {
                e.stopPropagation();
                setActiveIndex(prev => prev - 1);
                setFlipped(false);
            }}
            disabled={activeIndex === 0}
          >
            ←
          </button>
          <div style={{ fontSize: '1.1rem', fontWeight: '800', color: '#1e293b', minWidth: '80px', textAlign: 'center' }}>
            {activeIndex + 1} / {flashcards.length}
          </div>
          <button
            className="nav-circle-btn"
            onClick={(e) => {
                e.stopPropagation();
                setActiveIndex(prev => prev + 1);
                setFlipped(false);
            }}
            disabled={activeIndex === flashcards.length - 1}
          >
            →
          </button>
        </div>
      )}
    </div>
  );
}

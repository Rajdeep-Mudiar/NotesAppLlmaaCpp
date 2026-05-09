import React, { useState } from "react";
import "../styles/roadmap.css"; // We can reuse the roadmap layout styles

export default function Summarizer({ notes, onSummarize, busy }) {
  const [configuring, setConfiguring] = useState(true);
  const [selectedNoteIds, setSelectedNoteIds] = useState([]);
  const [summaryData, setSummaryData] = useState(null);

  const handleToggleNote = (id) => {
    setSelectedNoteIds(prev => 
      prev.includes(id) ? prev.filter(i => i !== id) : [...prev, id]
    );
  };

  const handleGenerate = async () => {
    if (busy) return;
    const result = await onSummarize(selectedNoteIds);
    if (result && !result.error) {
        setSummaryData(result);
        setConfiguring(false);
    }
  };

  if (configuring) {
    return (
      <div className="roadmap-view" style={{ padding: '2rem' }}>
        <div className="setup-container">
          <header style={{ marginBottom: '2.5rem', textAlign: 'center' }}>
            <h1 style={{ fontSize: '2rem', fontWeight: '800', color: '#0f172a', marginBottom: '0.5rem' }}>AI Summarizer</h1>
            <p style={{ color: '#64748b' }}>Select notes to generate a comprehensive summary</p>
          </header>

          <div className="setup-step">
            <label><span className="step-num">1</span> Select Source Notes ({selectedNoteIds.length})</label>
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
              {notes.length === 0 && <p style={{ padding: '20px', textAlign: 'center', color: '#94a3b8' }}>No notes available to summarize.</p>}
            </div>
          </div>

          <button 
            className="btn-add" 
            style={{ width: '100%', height: '60px', borderRadius: '18px', fontSize: '1.1rem', fontWeight: '700', marginTop: '1rem', background: '#f59e0b' }}
            disabled={busy || selectedNoteIds.length === 0}
            onClick={handleGenerate}
          >
            {busy ? "⏳ Summarizing..." : "✨ Generate Summary"}
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="roadmap-view">
      <div className="header-bar" style={{ padding: '1.5rem 2rem', background: 'white', borderBottom: '1px solid #f1f5f9' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{ background: '#f59e0b', color: 'white', padding: '8px', borderRadius: '10px' }}>✨</div>
            <h3 style={{ fontWeight: '800' }}>AI Summary</h3>
        </div>
        <button className="btn-add" style={{ background: '#f1f5f9', color: '#475569', fontSize: '0.9rem' }} onClick={() => { setConfiguring(true); setSummaryData(null); setSelectedNoteIds([]); }}>
          New Summary
        </button>
      </div>

      <div className="roadmap-container" style={{ padding: '2rem' }}>
        <div style={{ maxWidth: '800px', margin: '0 auto', background: '#f8fafc', borderRadius: '12px', padding: '2rem', border: '1px solid #e2e8f0', boxShadow: '0 4px 6px -1px rgba(0, 0, 0, 0.05)' }}>
            <p style={{ fontSize: '1.1rem', color: '#334155', lineHeight: '1.7', marginBottom: '2rem' }}>
                {summaryData?.overall_summary}
            </p>
            
            <div style={{ display: 'flex', flexDirection: 'column', gap: '1.5rem' }}>
                {summaryData?.sections && summaryData.sections.map((sec, i) => (
                    <div key={i} style={{ padding: '1.5rem', background: 'white', borderRadius: '10px', border: '1px solid #e2e8f0', borderLeft: '4px solid #f59e0b' }}>
                        <h4 style={{ margin: '0 0 0.8rem 0', color: '#0f172a', fontSize: '1.15rem' }}>{sec.heading}</h4>
                        <p style={{ margin: 0, color: '#475569', fontSize: '1rem', lineHeight: '1.6' }}>{sec.summary}</p>
                    </div>
                ))}
            </div>
        </div>
      </div>
    </div>
  );
}

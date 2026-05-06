import React, { useState } from "react";
import "../styles/roadmap.css";

export default function Roadmap({ roadmap, notes, onGenerateRoadmap, busy }) {
  const [configuring, setConfiguring] = useState(!roadmap || roadmap.length === 0);
  const [selectedNoteIds, setSelectedNoteIds] = useState([]);
  const [expandedModules, setExpandedModules] = useState([]);
  const [expandedSubModules, setExpandedSubModules] = useState([]);

  const toggleModule = (index) => {
    setExpandedModules(prev => 
      prev.includes(index) ? prev.filter(i => i !== index) : [...prev, index]
    );
  };

  const toggleSubModule = (modIdx, subIdx) => {
    const key = `${modIdx}-${subIdx}`;
    setExpandedSubModules(prev => 
      prev.includes(key) ? prev.filter(k => k !== key) : [...prev, key]
    );
  };

  const handleToggleNote = (id) => {
    setSelectedNoteIds(prev => 
      prev.includes(id) ? prev.filter(i => i !== id) : [...prev, id]
    );
  };

  const handleGenerate = async () => {
    if (busy) return;
    await onGenerateRoadmap(selectedNoteIds);
    setConfiguring(false);
  };

  if (configuring) {
    return (
      <div className="roadmap-view" style={{ padding: '2rem' }}>
        <div className="setup-container">
          <header style={{ marginBottom: '2.5rem', textAlign: 'center' }}>
            <h1 style={{ fontSize: '2rem', fontWeight: '800', color: '#0f172a', marginBottom: '0.5rem' }}>Learning Path</h1>
            <p style={{ color: '#64748b' }}>Select notes to generate a personalized roadmap</p>
          </header>

          <div className="setup-step">
            <label><span className="step-num">1</span> Source Notes ({selectedNoteIds.length})</label>
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
            {busy ? "🗺️ Plotting Course..." : "Generate Roadmap"}
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="roadmap-view">
      <div className="header-bar" style={{ padding: '1.5rem 2rem', background: 'white', borderBottom: '1px solid #f1f5f9' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{ background: 'var(--accent)', color: 'white', padding: '8px', borderRadius: '10px' }}>🗺️</div>
            <h3 style={{ fontWeight: '800' }}>Your Learning Journey</h3>
        </div>
        <button className="btn-add" style={{ background: '#f1f5f9', color: '#475569', fontSize: '0.9rem' }} onClick={() => setConfiguring(true)}>
          New Roadmap
        </button>
      </div>

      <div className="roadmap-container">
        <div className="timeline">
          {roadmap.map((module, modIdx) => (
            <div key={modIdx} className="timeline-item">
              <div className="timeline-marker">
                <div className="marker-circle">{modIdx + 1}</div>
                {modIdx < roadmap.length - 1 && <div className="marker-line"></div>}
              </div>
              <div 
                className={`timeline-content ${expandedModules.includes(modIdx) ? 'expanded' : ''}`}
                onClick={() => toggleModule(modIdx)}
              >
                <div className="step-header">
                  <h4 className="step-title">{module.title}</h4>
                  <span className={`expand-icon ${expandedModules.includes(modIdx) ? 'open' : ''}`}>▼</span>
                </div>
                <p className="step-desc">{module.description}</p>
                
                {expandedModules.includes(modIdx) && module.sub_modules && (
                  <div className="sub-modules-list" onClick={(e) => e.stopPropagation()}>
                    {module.sub_modules.map((sub, subIdx) => {
                        const isSubExpanded = expandedSubModules.includes(`${modIdx}-${subIdx}`);
                        return (
                            <div key={subIdx} className="sub-module-item">
                                <div className="sub-module-header" onClick={() => toggleSubModule(modIdx, subIdx)}>
                                    <span style={{ fontWeight: '700', color: 'var(--accent)' }}>{sub.title}</span>
                                    <span className={`expand-icon small ${isSubExpanded ? 'open' : ''}`}>▼</span>
                                </div>
                                {isSubExpanded && (
                                    <div className="sub-module-details">
                                        <p style={{ margin: '8px 0', fontSize: '0.9rem', color: '#64748b' }}>{sub.description}</p>
                                        <ul className="details-list">
                                            {sub.details && sub.details.map((detail, dIdx) => (
                                                <li key={dIdx}>{detail}</li>
                                            ))}
                                        </ul>
                                    </div>
                                )}
                            </div>
                        );
                    })}
                  </div>
                )}
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

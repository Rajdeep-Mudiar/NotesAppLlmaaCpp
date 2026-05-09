import React, { useState } from "react";
import "../styles/roadmap.css";

export default function Roadmap({ roadmap, history, notes, onLoadHistory, onGenerateRoadmap, busy }) {
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
      <div className="roadmap-view" style={{ padding: '2rem', display: 'flex', gap: '2rem', alignItems: 'flex-start' }}>
        <div className="setup-container" style={{ flex: 2 }}>
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

        <div className="setup-container" style={{ flex: 1, background: '#f8fafc', border: '1px dashed #cbd5e1' }}>
          <h3 style={{ marginBottom: '1.5rem', fontWeight: '800', color: '#334155' }}>Recent History</h3>
          {history && history.length > 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
              {history.slice().reverse().map(item => (
                <div 
                  key={item.id} 
                  className="history-card"
                  style={{ padding: '15px', background: 'white', borderRadius: '12px', border: '1px solid #e2e8f0', cursor: 'pointer', transition: 'all 0.2s' }}
                  onClick={() => { onLoadHistory(item); setConfiguring(false); }}
                  onMouseEnter={(e) => { e.currentTarget.style.borderColor = 'var(--accent)'; e.currentTarget.style.transform = 'translateY(-2px)'; }}
                  onMouseLeave={(e) => { e.currentTarget.style.borderColor = '#e2e8f0'; e.currentTarget.style.transform = 'translateY(0)'; }}
                >
                  <div style={{ fontWeight: '700', color: '#0f172a' }}>{item.roadmap?.length || 0} Modules</div>
                  <div style={{ fontSize: '0.8rem', color: '#64748b', marginTop: '4px' }}>
                    {new Date(item.created_at).toLocaleString()}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <p style={{ color: '#94a3b8', fontSize: '0.9rem', textAlign: 'center' }}>No recent roadmaps.</p>
          )}
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
                  <div>
                    <h4 className="step-title">{module.module_title}</h4>
                    <div style={{ fontSize: '0.8rem', color: '#64748b', fontWeight: '600', marginTop: '4px' }}>
                        Total Time: {module.total_estimated_time}
                    </div>
                  </div>
                  <span className={`expand-icon ${expandedModules.includes(modIdx) ? 'open' : ''}`}>▼</span>
                </div>
                <p className="step-desc">{module.description}</p>
                
                {expandedModules.includes(modIdx) && module.sections && (
                  <div className="sections-list" onClick={(e) => e.stopPropagation()}>
                    {module.sections.map((section, secIdx) => {
                        const secKey = `${modIdx}-${secIdx}`;
                        const isSecExpanded = expandedSubModules.includes(secKey);
                        return (
                            <div key={secIdx} className="section-item">
                                <div className="section-header" onClick={() => toggleSubModule(modIdx, secIdx)}>
                                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                                        <span style={{ fontWeight: '800', color: '#1e293b' }}>{section.section_title}</span>
                                        <span className="step-time small">{section.estimated_time}</span>
                                    </div>
                                    <span className={`expand-icon small ${isSecExpanded ? 'open' : ''}`}>▼</span>
                                </div>
                                {isSecExpanded && (
                                    <div className="topics-list">
                                        {section.topics && section.topics.map((topic, topIdx) => (
                                            <div key={topIdx} className="topic-card">
                                                <div className="topic-header">
                                                    <h5 className="topic-name">{topic.topic_name}</h5>
                                                    <div className="topic-meta">
                                                        <span className={`difficulty-badge ${topic.difficulty?.toLowerCase()}`}>
                                                            {topic.difficulty}
                                                        </span>
                                                        <span className="topic-duration">{topic.estimated_time}</span>
                                                    </div>
                                                </div>
                                                <p className="topic-desc">{topic.description}</p>
                                                {topic.learning_outcome && (
                                                    <div className="learning-outcome">
                                                        <strong>Outcome:</strong> {topic.learning_outcome}
                                                    </div>
                                                )}
                                                {topic.prerequisites && topic.prerequisites.length > 0 && (
                                                    <div className="prereqs">
                                                        <strong>Prereqs:</strong> {topic.prerequisites.join(', ')}
                                                    </div>
                                                )}
                                            </div>
                                        ))}
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

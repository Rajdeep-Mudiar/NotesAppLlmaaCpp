import React, { useState } from "react";
import "../styles/flashcards.css"; // Reuse the premium styles

export default function Quiz({ quiz, history, notes, onLoadHistory, onGenerateQuiz, busy }) {
  const [activeIndex, setActiveIndex] = useState(0);
  const [configuring, setConfiguring] = useState(true);
  const [selectedAnswer, setSelectedAnswer] = useState(null);
  const [isCorrect, setIsCorrect] = useState(null);
  const [score, setScore] = useState(0);
  const [showResults, setShowResults] = useState(false);

  // Form states
  const [count, setCount] = useState(5);
  const [difficulty, setDifficulty] = useState("medium");
  const [selectedNoteIds, setSelectedNoteIds] = useState([]);

  const activeQuestion = quiz[activeIndex] || null;
  const progress = quiz.length > 0 ? ((activeIndex + 1) / quiz.length) * 100 : 0;

  const handleToggleNote = (id) => {
    setSelectedNoteIds(prev => 
      prev.includes(id) ? prev.filter(i => i !== id) : [...prev, id]
    );
  };

  const handleGenerate = async () => {
    if (busy) return;
    await onGenerateQuiz(count, difficulty, selectedNoteIds);
    setActiveIndex(0);
    setSelectedAnswer(null);
    setIsCorrect(null);
    setScore(0);
    setShowResults(false);
    setConfiguring(false);
  };

  const handleAnswer = (option) => {
    if (selectedAnswer !== null) return;
    setSelectedAnswer(option);
    const correct = option === activeQuestion.answer;
    setIsCorrect(correct);
    if (correct) setScore(prev => prev + 1);
  };

  const handleNext = () => {
    if (activeIndex < quiz.length - 1) {
      setActiveIndex(prev => prev + 1);
      setSelectedAnswer(null);
      setIsCorrect(null);
    } else {
      setShowResults(true);
    }
  };

  if (configuring) {
    return (
      <div className="flashcards-view" style={{ padding: '2rem', display: 'flex', gap: '2rem', alignItems: 'flex-start' }}>
        <div className="setup-container" style={{ flex: 2 }}>
          <header style={{ marginBottom: '2.5rem', textAlign: 'center' }}>
            <h1 style={{ fontSize: '2rem', fontWeight: '800', color: '#0f172a', marginBottom: '0.5rem' }}>Quiz Arena</h1>
            <p style={{ color: '#64748b' }}>Test your knowledge with AI-generated MCQs</p>
          </header>

          <div className="setup-step">
            <label><span className="step-num">1</span> Question Count</label>
            <div style={{ display: 'flex', alignItems: 'center', gap: '15px' }}>
                <input 
                    type="range" min="3" max="15" step="1"
                    style={{ flex: 1, accentColor: 'var(--accent)' }}
                    value={count}
                    onChange={e => setCount(parseInt(e.target.value))}
                />
                <span style={{ fontWeight: '800', fontSize: '1.2rem', color: 'var(--accent)', minWidth: '30px' }}>{count}</span>
            </div>
          </div>

          <div className="setup-step">
            <label><span className="step-num">2</span> Level</label>
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
            <label><span className="step-num">3</span> Source Material ({selectedNoteIds.length})</label>
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
            </div>
          </div>

          <button 
            className="btn-add" 
            style={{ width: '100%', height: '60px', borderRadius: '18px', fontSize: '1.1rem', fontWeight: '700' }}
            disabled={busy || notes.length === 0}
            onClick={handleGenerate}
          >
            {busy ? "⚙️ Generating Arena..." : "Start Quiz"}
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
                  onClick={() => { 
                    onLoadHistory(item); 
                    setActiveIndex(0); 
                    setSelectedAnswer(null); 
                    setIsCorrect(null); 
                    setScore(0); 
                    setShowResults(false); 
                    setConfiguring(false); 
                  }}
                  onMouseEnter={(e) => { e.currentTarget.style.borderColor = 'var(--accent)'; e.currentTarget.style.transform = 'translateY(-2px)'; }}
                  onMouseLeave={(e) => { e.currentTarget.style.borderColor = '#e2e8f0'; e.currentTarget.style.transform = 'translateY(0)'; }}
                >
                  <div style={{ fontWeight: '700', color: '#0f172a' }}>{item.questions?.length || 0} Questions</div>
                  <div style={{ fontSize: '0.8rem', color: '#64748b', marginTop: '4px' }}>
                    {new Date(item.created_at).toLocaleString()}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <p style={{ color: '#94a3b8', fontSize: '0.9rem', textAlign: 'center' }}>No recent quiz sessions.</p>
          )}
        </div>
      </div>
    );
  }

  if (showResults) {
      return (
        <div className="flashcards-view" style={{ display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
            <div className="setup-container" style={{ textAlign: 'center', padding: '4rem 2rem' }}>
                <div style={{ fontSize: '4rem', marginBottom: '1.5rem' }}>🏆</div>
                <h2 style={{ fontWeight: '800', marginBottom: '1rem' }}>Quiz Complete!</h2>
                <div style={{ fontSize: '2.5rem', fontWeight: '900', color: 'var(--accent)', marginBottom: '1rem' }}>
                    {score} / {quiz.length}
                </div>
                <p style={{ color: '#64748b', marginBottom: '2rem' }}>
                    {score === quiz.length ? "Perfect score! You've mastered these notes." : "Great effort! Review your notes and try again."}
                </p>
                <button className="btn-add" style={{ padding: '1rem 3rem' }} onClick={() => setConfiguring(true)}>New Quiz</button>
            </div>
        </div>
      )
  }

  return (
    <div className="flashcards-view">
      <div className="header-bar" style={{ padding: '1.5rem 2rem', background: 'white', borderBottom: '1px solid #f1f5f9' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <div style={{ background: 'var(--accent)', color: 'white', padding: '8px', borderRadius: '10px' }}>🎯</div>
            <h3 style={{ fontWeight: '800' }}>Quiz Arena</h3>
        </div>
        <div style={{ fontWeight: '700', color: 'var(--accent)' }}>Score: {score}</div>
      </div>

      <div className="card-scene" style={{ justifyContent: 'flex-start', paddingTop: '4rem' }}>
        {activeQuestion && (
           <div className="progress-container">
             <div className="progress-bar" style={{ width: `${progress}%` }}></div>
           </div>
        )}

        {activeQuestion && (
            <div style={{ width: '100%', maxWidth: '600px' }}>
                <div className="setup-container" style={{ padding: '2.5rem', marginBottom: '2rem' }}>
                    <span className="card-tag" style={{ position: 'relative', top: 0, marginBottom: '1rem', display: 'block', color: 'var(--accent)' }}>QUESTION {activeIndex + 1}</span>
                    <h2 style={{ fontSize: '1.4rem', fontWeight: '700', lineHeight: '1.4', color: '#1e293b' }}>
                        {activeQuestion.question}
                    </h2>
                </div>

                <div style={{ display: 'grid', gap: '12px' }}>
                    {activeQuestion.options.map((opt, i) => {
                        const isSelected = selectedAnswer === opt;
                        const isCorrectOpt = opt === activeQuestion.answer;
                        let bg = 'white';
                        let border = '2px solid #f1f5f9';
                        let color = '#475569';

                        if (selectedAnswer !== null) {
                            if (isCorrectOpt) {
                                bg = '#dcfce7';
                                border = '2px solid #22c55e';
                                color = '#166534';
                            } else if (isSelected) {
                                bg = '#fee2e2';
                                border = '2px solid #ef4444';
                                color = '#991b1b';
                            }
                        }

                        return (
                            <button 
                                key={i}
                                onClick={() => handleAnswer(opt)}
                                disabled={selectedAnswer !== null}
                                style={{ 
                                    padding: '1.2rem 1.5rem', borderRadius: '16px', border, background: bg,
                                    textAlign: 'left', cursor: selectedAnswer ? 'default' : 'pointer',
                                    fontSize: '1rem', fontWeight: '600', color, transition: 'all 0.2s',
                                    display: 'flex', alignItems: 'center', gap: '15px'
                                }}
                            >
                                <div style={{ 
                                    width: '32px', height: '32px', borderRadius: '50%', background: isSelected ? 'currentColor' : '#f1f5f9',
                                    display: 'flex', alignItems: 'center', justifyContent: 'center', fontSize: '0.8rem',
                                    color: isSelected ? 'white' : '#64748b'
                                }}>
                                    {String.fromCharCode(65 + i)}
                                </div>
                                {opt}
                            </button>
                        );
                    })}
                </div>

                {selectedAnswer && (
                    <button 
                        className="btn-add" 
                        style={{ width: '100%', marginTop: '2rem', height: '56px', borderRadius: '16px' }}
                        onClick={handleNext}
                    >
                        {activeIndex === quiz.length - 1 ? "View Results" : "Next Question"}
                    </button>
                )}
            </div>
        )}
      </div>
    </div>
  );
}

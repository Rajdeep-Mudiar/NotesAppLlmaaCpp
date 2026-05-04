import React, { useMemo, useState } from "react";

export default function Flashcards({ flashcards, onGenerateFlashcards, busy }) {
  const [activeIndex, setActiveIndex] = useState(0);
  const [flipped, setFlipped] = useState(false);
  const [count, setCount] = useState(5);

  const activeCard = flashcards[activeIndex] || null;

  const handleGenerate = async () => {
    if (busy) return;
    await onGenerateFlashcards(count);
    setActiveIndex(0);
    setFlipped(false);
  };

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", background: "var(--bg)" }}>
      <div className="header-bar">
        <h3>Memory Brain</h3>
        <div className="controls-row" style={{ alignItems: "center" }}>
          <span style={{ fontSize: "0.85rem", color: "var(--text-muted)", fontWeight: '600' }}>Cards:</span>
          <input
            type="number"
            className="select-modern"
            style={{ width: "60px", background: '#F0F2F5', border: 'none', borderRadius: '8px' }}
            value={count}
            min="1"
            max="20"
            onChange={(e) => setCount(parseInt(e.target.value) || 5)}
          />
          <button className="btn-add" onClick={handleGenerate} disabled={busy}>
            {busy ? "Thinking..." : "Generate Deck"}
          </button>
        </div>
      </div>

      <div style={{ flex: 1, display: "flex", alignItems: "center", justifyContent: "center", padding: "40px" }}>
        {activeCard ? (
          <div
            style={{
              width: "100%",
              maxWidth: "450px",
              height: "550px",
              perspective: "1200px",
              cursor: "pointer",
            }}
            onClick={() => setFlipped(!flipped)}
          >
            <div
              style={{
                position: "relative",
                width: "100%",
                height: "100%",
                transition: "transform 0.8s cubic-bezier(0.175, 0.885, 0.32, 1.275)",
                transformStyle: "preserve-3d",
                transform: flipped ? "rotateY(180deg)" : "rotateY(0deg)",
              }}
            >
              {/* Front Side */}
              <div
                style={{
                  position: "absolute",
                  inset: 0,
                  backfaceVisibility: "hidden",
                  background: "white",
                  boxShadow: 'var(--shadow-lg)',
                  borderRadius: "24px",
                  display: "flex",
                  flexDirection: 'column',
                  alignItems: "center",
                  justifyContent: "center",
                  padding: "48px",
                  textAlign: "center",
                }}
              >
                <div style={{ position: 'absolute', top: '24px', fontSize: '0.8rem', color: 'var(--accent)', fontWeight: '700', letterSpacing: '0.1em' }}>QUESTION</div>
                <div style={{ fontSize: "1.5rem", fontWeight: "700", color: '#050505', lineHeight: '1.4' }}>
                  {activeCard.front}
                </div>
                <div style={{ position: 'absolute', bottom: '24px', fontSize: '0.8rem', color: 'var(--text-muted)' }}>Tap to reveal answer</div>
              </div>

              {/* Back Side */}
              <div
                style={{
                  position: "absolute",
                  inset: 0,
                  backfaceVisibility: "hidden",
                  background: "var(--accent-gradient)",
                  color: "white",
                  boxShadow: 'var(--shadow-lg)',
                  borderRadius: "24px",
                  display: "flex",
                  flexDirection: 'column',
                  alignItems: "center",
                  justifyContent: "center",
                  padding: "48px",
                  textAlign: "center",
                  transform: "rotateY(180deg)",
                }}
              >
                 <div style={{ position: 'absolute', top: '24px', fontSize: '0.8rem', color: 'rgba(255,255,255,0.8)', fontWeight: '700', letterSpacing: '0.1em' }}>ANSWER</div>
                 <div style={{ fontSize: "1.3rem", fontWeight: "600", lineHeight: '1.6' }}>
                  {activeCard.back}
                </div>
              </div>
            </div>
          </div>
        ) : (
          <div style={{ textAlign: "center", background: 'white', padding: '40px', borderRadius: '24px', boxShadow: 'var(--shadow)' }}>
            <div style={{ fontSize: '3rem', marginBottom: '16px' }}>📚</div>
            <p style={{ color: "var(--text-muted)", fontWeight: '600' }}>Your knowledge deck is empty.</p>
            <p style={{ color: "var(--text-muted)", fontSize: '0.9rem', marginTop: '8px' }}>Generate cards from your notes to start active recall.</p>
          </div>
        )}
      </div>

      {flashcards.length > 0 && (
        <div style={{ padding: "32px", display: "flex", justifyContent: "center", gap: "24px" }}>
          <button
            className="nav-item"
            style={{ padding: '8px 24px', borderRadius: '20px', border: '1px solid var(--border)' }}
            onClick={(e) => {
                e.stopPropagation();
                setActiveIndex((prev) => (prev > 0 ? prev - 1 : prev));
                setFlipped(false);
            }}
            disabled={activeIndex === 0}
          >
            ← Back
          </button>
          <div style={{ display: "flex", alignItems: "center", fontWeight: "700", color: 'var(--text-muted)', fontSize: '0.9rem' }}>
            {activeIndex + 1} / {flashcards.length}
          </div>
          <button
            className="nav-item"
            style={{ padding: '8px 24px', borderRadius: '20px', border: '1px solid var(--border)' }}
            onClick={(e) => {
                e.stopPropagation();
                setActiveIndex((prev) => (prev < flashcards.length - 1 ? prev + 1 : prev));
                setFlipped(false);
            }}
            disabled={activeIndex === flashcards.length - 1}
          >
            Next →
          </button>
        </div>
      )}
    </div>
  );
}

import React, { useMemo, useState } from "react";

export default function Flashcards({
  flashcards,
  ideas,
  learningPath,
  selfQuestions,
}) {
  const [activeIndex, setActiveIndex] = useState(0);
  const [flipped, setFlipped] = useState(false);

  const activeCard = flashcards[activeIndex] || null;

  const learningPreview = useMemo(
    () => learningPath.slice(0, 4),
    [learningPath],
  );

  return (
    <section className="panel-grid flashcards-layout">
      <div className="panel flashcard-stage">
        <div className="panel-header">
          <div>
            <p className="panel-kicker">Flashcard generator</p>
            <h3>Turn notes into questions and answers</h3>
          </div>
          <div className="carousel-controls">
            <button
              type="button"
              className="ghost-button"
              onClick={() => setActiveIndex((value) => Math.max(0, value - 1))}
            >
              Prev
            </button>
            <button
              type="button"
              className="ghost-button"
              onClick={() =>
                setActiveIndex((value) =>
                  Math.min(flashcards.length - 1, value + 1),
                )
              }
            >
              Next
            </button>
          </div>
        </div>

        {activeCard ? (
          <div
            className={flipped ? "flip-card flipped" : "flip-card"}
            onClick={() => setFlipped((value) => !value)}
          >
            <div className="flip-face flip-front">
              <span>Front</span>
              <h4>{activeCard.front}</h4>
              <p>Tap to reveal the answer.</p>
            </div>
            <div className="flip-face flip-back">
              <span>Back</span>
              <h4>{activeCard.back}</h4>
            </div>
          </div>
        ) : (
          <div className="empty-state">
            No flashcards yet. Add more notes and the backend will generate them
            automatically.
          </div>
        )}
      </div>

      <aside className="side-column">
        <div className="panel compact-panel">
          <p className="panel-kicker">Learning path</p>
          <ul className="compact-list">
            {learningPreview.map((step) => (
              <li key={`${step.step}-${step.title}`}>
                <strong>{step.title}</strong>
                <span>{step.reason}</span>
              </li>
            ))}
          </ul>
        </div>

        <div className="panel compact-panel">
          <p className="panel-kicker">Self questions</p>
          <ul className="compact-list">
            {selfQuestions.slice(0, 4).map((item, index) => (
              <li key={`${item.question}-${index}`}>{item.question}</li>
            ))}
          </ul>
        </div>

        <div className="panel compact-panel">
          <p className="panel-kicker">Idea generation</p>
          <ul className="compact-list">
            {ideas.slice(0, 4).map((idea, index) => (
              <li key={`${idea.title}-${index}`}>
                <strong>{idea.title}</strong>
                <span>{idea.idea}</span>
              </li>
            ))}
          </ul>
        </div>
      </aside>
    </section>
  );
}

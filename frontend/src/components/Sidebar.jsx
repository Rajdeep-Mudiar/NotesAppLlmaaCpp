import React from "react";

export default function Sidebar({ views, activeView, onSwitch }) {
  const getIcon = (id) => {
    switch (id) {
      case "notes": return "📝";
      case "chat": return "💬";
      case "flashcards": return "🗂️";
      case "quiz": return "🎯";
      case "graph": return "🕸️";
      case "roadmap": return "🗺️";
      case "learning": return "🎓";
      default: return "📄";
    }
  };

  return (
    <aside className="sidebar">
      <div className="logo">
        <div className="logo-icon">
          <svg
            width="24"
            height="24"
            viewBox="0 0 24 24"
            fill="none"
            stroke="white"
            strokeWidth="3"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M12 2L2 7l10 5 10-5-10-5z" />
          </svg>
        </div>
        <span>Brain</span>
      </div>

      <nav className="nav-links">
        {views.map((view) => (
          <button
            key={view.id}
            className={`nav-item ${activeView === view.id ? "active" : ""}`}
            onClick={() => onSwitch(view.id)}
          >
            <div className="icon-box">{getIcon(view.id)}</div>
            <span>{view.label}</span>
          </button>
        ))}
      </nav>

      <div className="sidebar-footer" style={{ marginTop: 'auto', padding: '16px' }}>
        <div className="tag" style={{ background: 'var(--accent-gradient)', color: 'white', border: 'none' }}>
          ✨ AI Enhanced
        </div>
      </div>
    </aside>
  );
}

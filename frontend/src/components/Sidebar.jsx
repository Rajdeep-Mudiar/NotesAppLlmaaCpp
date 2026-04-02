export default function Sidebar({
  activeView,
  setActiveView,
  views,
  stats,
  onRefresh,
  busy,
}) {
  return (
    <aside className="sidebar">
      <div className="brand-lockup">
        <div className="brand-mark">SB</div>
        <div>
          <h2>Second Brain</h2>
          <p>Local AI notes workspace</p>
        </div>
      </div>

      <nav className="sidebar-nav">
        {views.map((view) => (
          <button
            key={view.id}
            className={activeView === view.id ? "nav-item active" : "nav-item"}
            onClick={() => setActiveView(view.id)}
            type="button"
          >
            {view.label}
          </button>
        ))}
      </nav>

      <div className="sidebar-card">
        <span className="sidebar-label">Status</span>
        <strong>{busy ? "Syncing" : "Ready"}</strong>
        <p>{stats.notes} notes indexed for retrieval.</p>
        <button className="refresh-button" type="button" onClick={onRefresh}>
          Refresh workspace
        </button>
      </div>
    </aside>
  );
}

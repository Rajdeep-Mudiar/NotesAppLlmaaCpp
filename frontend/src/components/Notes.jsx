import React, { useState } from 'react';

const Notes = ({ notes, onSave, onDelete }) => {
  const [selectedNote, setSelectedNote] = useState(null);
  const [isEditing, setIsEditing] = useState(false);
  const [searchQuery, setSearchQuery] = useState("");

  const filteredNotes = notes.filter(n => 
    n.title.toLowerCase().includes(searchQuery.toLowerCase()) ||
    n.content.toLowerCase().includes(searchQuery.toLowerCase()) ||
    (n.tags && n.tags.some(t => t.toLowerCase().includes(searchQuery.toLowerCase())))
  );

  const getInitials = (title) => {
    return title ? title.split(' ').map(n => n[0]).join('').slice(0, 2).toUpperCase() : "UN";
  };

  const handleEdit = (note) => {
    setSelectedNote(note);
    setIsEditing(true);
  };

  const handleCreate = () => {
    setSelectedNote({ title: "", content: "", tags: [] });
    setIsEditing(true);
  };

  const handleSave = () => {
    if (!selectedNote.title || !selectedNote.content) {
        alert("Title and content are required.");
        return;
    }
    onSave(selectedNote);
    setIsEditing(false);
    setSelectedNote(null);
  };

  if (isEditing) {
    return (
      <div className="editor-pane">
        <div className="editor-header">
          <input 
            className="editor-title-input"
            value={selectedNote.title}
            onChange={(e) => setSelectedNote({...selectedNote, title: e.target.value})}
            placeholder="What's on your mind?"
          />
          <div className="editor-footer">
            <button className="btn-add" onClick={handleSave}>Post Note</button>
            <button className="btn-add" style={{background: '#E4E6EB', color: '#050505'}} onClick={() => setIsEditing(false)}>Cancel</button>
          </div>
        </div>
        <textarea 
          className="editor-content"
          value={selectedNote.content}
          onChange={(e) => setSelectedNote({...selectedNote, content: e.target.value})}
          placeholder="Start typing your thoughts..."
        />
        <div style={{marginTop: '1rem', display: 'flex', gap: '8px', alignItems: 'center'}}>
           <span style={{fontSize: '0.9rem', color: 'var(--text-muted)'}}>Tags:</span>
           <input 
            className="search-bar"
            style={{flex: 1, borderRadius: '8px', padding: '6px 12px'}}
            value={selectedNote.tags ? selectedNote.tags.join(", ") : ""}
            onChange={(e) => setSelectedNote({...selectedNote, tags: e.target.value.split(",").map(t => t.trim())})}
            placeholder="Add tags separated by commas..."
          />
        </div>
      </div>
    );
  }

  return (
    <div className="notes-container">
      <div className="notes-controls">
        <div className="logo-icon" style={{background: '#E4E6EB', color: '#050505', minWidth: '40px'}}>Me</div>
        <input 
          className="search-bar"
          placeholder="Search through your thoughts..."
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
        />
        <button className="btn-add" onClick={handleCreate}>
          New Thought
        </button>
      </div>

      <div className="notes-list">
        {filteredNotes.map(note => (
          <div key={note.id} className="note-card" onClick={() => handleEdit(note)}>
            <div style={{display: 'flex', gap: '10px', alignItems: 'center', marginBottom: '10px'}}>
              <div className="logo-icon" style={{background: 'var(--accent-soft)', color: 'var(--accent)', minWidth: '32px', height: '32px', fontSize: '0.8rem'}}>
                {getInitials(note.title)}
              </div>
              <h3 style={{margin: 0, fontSize: '0.95rem'}} title={note.title}>{note.title || "Untitled"}</h3>
            </div>
            
            <p>{note.content}</p>
            
            <div className="note-tags">
              {note.tags && note.tags.slice(0, 2).map(tag => (
                <span key={tag} className="tag" style={{padding: '2px 8px', fontSize: '0.7rem'}}>{tag}</span>
              ))}
            </div>

            <div className="card-actions">
              <button 
                className="action-btn"
                onClick={(e) => { e.stopPropagation(); handleEdit(note); }}
                title="Edit Thought"
              >
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4L18.5 2.5z"></path></svg>
              </button>
              <button 
                className="action-btn delete"
                onClick={(e) => { e.stopPropagation(); onDelete(note.id); }}
                title="Delete Thought"
              >
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path><line x1="10" y1="11" x2="10" y2="17"></line><line x1="14" y1="11" x2="14" y2="17"></line></svg>
              </button>
            </div>
          </div>
        ))}
        {filteredNotes.length === 0 && (
          <div style={{textAlign: 'center', padding: '4rem', background: 'var(--panel)', borderRadius: 'var(--radius-md)', boxShadow: 'var(--shadow)'}}>
            <p style={{color: 'var(--text-muted)'}}>No thoughts found. Start sharing your ideas!</p>
          </div>
        )}
      </div>
    </div>
  );
};

export default Notes;

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
            <div style={{display: 'flex', gap: '12px', alignItems: 'flex-start', marginBottom: '12px'}}>
              <div className="logo-icon" style={{background: 'var(--accent-soft)', color: 'var(--accent)', minWidth: '40px'}}>
                {getInitials(note.title)}
              </div>
              <div>
                <h3 style={{margin: 0}}>{note.title || "Untitled Note"}</h3>
                <span style={{fontSize: '0.8rem', color: 'var(--text-muted)'}}>{note.updated_at || "Just now"}</span>
              </div>
              {note.id && (
                <button 
                    onClick={(e) => { e.stopPropagation(); onDelete(note.id); }}
                    style={{marginLeft: 'auto', background: 'none', border: 'none', color: 'var(--text-muted)', cursor: 'pointer', fontSize: '1.2rem'}}
                    title="Delete"
                >
                    ×
                </button>
              )}
            </div>
            <p>{note.content}</p>
            <div className="note-tags">
              {note.tags && note.tags.slice(0, 5).map(tag => (
                <span key={tag} className="tag">{tag}</span>
              ))}
              {note.tags && note.tags.length > 5 && <span className="tag">+{note.tags.length - 5} more</span>}
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

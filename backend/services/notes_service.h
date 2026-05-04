#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct NoteRecord {
    std::string id;
    std::string title;
    std::string content;
    std::vector<std::string> tags;
    std::string created_at;
    std::string updated_at;
};

class NotesService {
public:
    explicit NotesService(std::string base_directory, std::string executable_dir = "");

    NoteRecord addNote(const std::string & title, const std::string & content, const std::vector<std::string> & tags = {});
    bool updateNote(const std::string & id, const std::string & title, const std::string & content, const std::vector<std::string> & tags = {});
    bool deleteNote(const std::string & id);

    std::vector<NoteRecord> loadNotes() const;
    std::vector<NoteRecord> searchNotes(const std::string & query, std::size_t limit = 5) const;
    std::vector<std::string> allTags() const;
    nlohmann::json noteHistory(const std::string & id) const;
    bool restoreVersion(const std::string & id, const std::string & version_file);

    std::string baseDirectory() const;
    std::string notePath(const std::string & id) const;
    std::string historyDirectory(const std::string & id) const;

private:
    std::string base_directory_;
    std::string executable_dir_;

    static std::string nowIso8601();
    static std::string newId();
    static std::string slugify(const std::string & value);
    static std::vector<std::string> normalizeTags(const std::vector<std::string> & tags);
    static std::vector<std::string> tokenize(const std::string & text);
    static std::vector<std::string> generateTags(const std::string & title, const std::string & content);
    static double relevanceScore(const NoteRecord & note, const std::vector<std::string> & query_tokens);
    static std::string toLower(std::string value);
    static std::string snippet(const std::string & content, std::size_t max_length = 220);

    NoteRecord readNoteFile(const std::string & path) const;
    void writeNoteFile(const NoteRecord & note) const;
    void snapshotNote(const std::string & id, const std::string & reason) const;
};

void to_json(nlohmann::json & json_value, const NoteRecord & note);
void from_json(const nlohmann::json & json_value, NoteRecord & note);
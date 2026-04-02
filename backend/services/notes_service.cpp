#include "notes_service.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <utility>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {
const std::vector<std::string> kStopWords = {
    "a", "an", "and", "are", "as", "at", "be", "but", "by", "for", "from", "has", "have", "how",
    "i", "if", "in", "is", "it", "its", "of", "on", "or", "our", "that", "the", "their", "then",
    "there", "this", "to", "was", "were", "what", "when", "where", "which", "with", "you", "your",
    "about", "into", "can", "should", "could", "would", "than", "them", "they", "we", "not", "no"
};

bool isStopWord(const std::string & token) {
    return std::find(kStopWords.begin(), kStopWords.end(), token) != kStopWords.end();
}
} // namespace

void to_json(nlohmann::json & json_value, const NoteRecord & note) {
    json_value = nlohmann::json{
        {"id", note.id},
        {"title", note.title},
        {"content", note.content},
        {"tags", note.tags},
        {"created_at", note.created_at},
        {"updated_at", note.updated_at},
    };
}

void from_json(const nlohmann::json & json_value, NoteRecord & note) {
    json_value.at("id").get_to(note.id);
    json_value.at("title").get_to(note.title);
    json_value.at("content").get_to(note.content);
    json_value.at("tags").get_to(note.tags);
    json_value.at("created_at").get_to(note.created_at);
    json_value.at("updated_at").get_to(note.updated_at);
}

NotesService::NotesService(std::string base_directory)
    : base_directory_(std::move(base_directory)) {
    fs::create_directories(base_directory_);
}

std::string NotesService::baseDirectory() const {
    return base_directory_;
}

std::string NotesService::notePath(const std::string & id) const {
    return (fs::path(base_directory_) / (slugify(id) + ".json")).string();
}

std::string NotesService::historyDirectory(const std::string & id) const {
    return (fs::path(base_directory_) / ".history" / slugify(id)).string();
}

std::string NotesService::nowIso8601() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time{};
#if defined(_WIN32)
    gmtime_s(&utc_time, &time);
#else
    gmtime_r(&time, &utc_time);
#endif
    std::ostringstream out;
    out << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string NotesService::newId() {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    static thread_local std::uniform_int_distribution<unsigned long long> dist(0, 999999);
    return std::to_string(millis) + "-" + std::to_string(dist(rng));
}

std::string NotesService::slugify(const std::string & value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_') {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else {
            result.push_back('_');
        }
    }
    return result;
}

std::string NotesService::toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::vector<std::string> NotesService::normalizeTags(const std::vector<std::string> & tags) {
    std::vector<std::string> result;
    for (std::string tag : tags) {
        tag = toLower(tag);
        tag.erase(std::remove_if(tag.begin(), tag.end(), [](unsigned char ch) { return !std::isalnum(ch); }), tag.end());
        if (!tag.empty() && !isStopWord(tag) && std::find(result.begin(), result.end(), tag) == result.end()) {
            result.push_back(tag);
        }
    }
    return result;
}

std::vector<std::string> NotesService::tokenize(const std::string & text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!current.empty()) {
            if (!isStopWord(current)) {
                tokens.push_back(current);
            }
            current.clear();
        }
    }
    if (!current.empty() && !isStopWord(current)) {
        tokens.push_back(current);
    }
    return tokens;
}

std::vector<std::string> NotesService::generateTags(const std::string & title, const std::string & content) {
    std::unordered_map<std::string, int> counts;
    for (const auto & token : tokenize(title + " " + content)) {
        ++counts[token];
    }

    std::vector<std::pair<std::string, int>> ranking(counts.begin(), counts.end());
    std::sort(ranking.begin(), ranking.end(), [](const auto & lhs, const auto & rhs) {
        if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
        }
        return lhs.first < rhs.first;
    });

    std::vector<std::string> tags;
    for (const auto & [token, count] : ranking) {
        (void)count;
        if (token.size() >= 4 && std::find(tags.begin(), tags.end(), token) == tags.end()) {
            tags.push_back(token);
        }
        if (tags.size() == 5) {
            break;
        }
    }
    if (tags.empty()) {
        tags.push_back("general");
    }
    return tags;
}

std::string NotesService::snippet(const std::string & content, std::size_t max_length) {
    if (content.size() <= max_length) {
        return content;
    }
    return content.substr(0, max_length - 3) + "...";
}

double NotesService::relevanceScore(const NoteRecord & note, const std::vector<std::string> & query_tokens) {
    if (query_tokens.empty()) {
        return 0.0;
    }

    const auto note_tokens = tokenize(note.title + " " + note.content);
    double score = 0.0;
    for (const auto & token : query_tokens) {
        if (note.title.find(token) != std::string::npos) {
            score += 1.25;
        }
        if (std::find(note.tags.begin(), note.tags.end(), token) != note.tags.end()) {
            score += 2.0;
        }
        score += std::count(note_tokens.begin(), note_tokens.end(), token) * 1.0;
    }
    score += std::min<std::size_t>(note.content.size() / 400, 5) * 0.2;
    return score;
}

NoteRecord NotesService::addNote(const std::string & title, const std::string & content, const std::vector<std::string> & tags) {
    NoteRecord note;
    note.id = newId();
    note.title = title.empty() ? "Untitled note" : title;
    note.content = content;
    note.tags = normalizeTags(tags);
    const auto auto_tags = generateTags(note.title, note.content);
    for (const auto & tag : auto_tags) {
        if (std::find(note.tags.begin(), note.tags.end(), tag) == note.tags.end()) {
            note.tags.push_back(tag);
        }
    }
    note.created_at = nowIso8601();
    note.updated_at = note.created_at;
    writeNoteFile(note);
    snapshotNote(note.id, "create");
    return note;
}

bool NotesService::updateNote(const std::string & id, const std::string & title, const std::string & content, const std::vector<std::string> & tags) {
    const auto path = notePath(id);
    if (!fs::exists(path)) {
        return false;
    }

    snapshotNote(id, "update");

    NoteRecord note = readNoteFile(path);
    note.title = title.empty() ? note.title : title;
    note.content = content;
    note.tags = normalizeTags(tags);
    const auto auto_tags = generateTags(note.title, note.content);
    for (const auto & tag : auto_tags) {
        if (std::find(note.tags.begin(), note.tags.end(), tag) == note.tags.end()) {
            note.tags.push_back(tag);
        }
    }
    note.updated_at = nowIso8601();
    writeNoteFile(note);
    return true;
}

bool NotesService::deleteNote(const std::string & id) {
    const auto path = notePath(id);
    if (fs::exists(path)) {
        snapshotNote(id, "delete");
    }
    std::error_code ec;
    return fs::remove(path, ec);
}

std::vector<NoteRecord> NotesService::loadNotes() const {
    std::vector<NoteRecord> notes;
    if (!fs::exists(base_directory_)) {
        return notes;
    }

    for (const auto & entry : fs::directory_iterator(base_directory_)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        try {
            notes.push_back(readNoteFile(entry.path().string()));
        } catch (...) {
        }
    }

    std::sort(notes.begin(), notes.end(), [](const NoteRecord & lhs, const NoteRecord & rhs) {
        return lhs.updated_at > rhs.updated_at;
    });
    return notes;
}

std::vector<NoteRecord> NotesService::searchNotes(const std::string & query, std::size_t limit) const {
    const auto query_tokens = tokenize(query);
    auto notes = loadNotes();
    std::sort(notes.begin(), notes.end(), [&query_tokens](const NoteRecord & lhs, const NoteRecord & rhs) {
        return relevanceScore(lhs, query_tokens) > relevanceScore(rhs, query_tokens);
    });
    if (notes.size() > limit) {
        notes.resize(limit);
    }
    return notes;
}

std::vector<std::string> NotesService::allTags() const {
    std::vector<std::string> tags;
    for (const auto & note : loadNotes()) {
        for (const auto & tag : note.tags) {
            if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
                tags.push_back(tag);
            }
        }
    }
    std::sort(tags.begin(), tags.end());
    return tags;
}

NoteRecord NotesService::readNoteFile(const std::string & path) const {
    std::ifstream file(path);
    nlohmann::json json_value;
    file >> json_value;
    return json_value.get<NoteRecord>();
}

void NotesService::writeNoteFile(const NoteRecord & note) const {
    fs::create_directories(base_directory_);
    std::ofstream file(notePath(note.id), std::ios::binary | std::ios::trunc);
    nlohmann::json json_value = note;
    file << json_value.dump(2);
}

nlohmann::json NotesService::noteHistory(const std::string & id) const {
    const auto history_dir = historyDirectory(id);
    nlohmann::json versions = nlohmann::json::array();
    if (!fs::exists(history_dir)) {
        return versions;
    }

    std::vector<fs::path> entries;
    for (const auto & entry : fs::directory_iterator(history_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            entries.push_back(entry.path());
        }
    }

    std::sort(entries.begin(), entries.end(), [](const fs::path & lhs, const fs::path & rhs) {
        return lhs.filename().string() > rhs.filename().string();
    });

    for (const auto & entry : entries) {
        try {
            std::ifstream file(entry.string());
            nlohmann::json payload;
            file >> payload;
            versions.push_back({
                {"file", entry.filename().string()},
                {"timestamp", payload.value("timestamp", "")},
                {"reason", payload.value("reason", "update")},
                {"title", payload["note"].value("title", "")},
                {"updated_at", payload["note"].value("updated_at", "")}
            });
        } catch (...) {
        }
    }

    return versions;
}

bool NotesService::restoreVersion(const std::string & id, const std::string & version_file) {
    const auto target = fs::path(historyDirectory(id)) / version_file;
    if (!fs::exists(target)) {
        return false;
    }

    std::ifstream file(target.string());
    nlohmann::json payload;
    file >> payload;
    if (!payload.contains("note")) {
        return false;
    }

    const auto current_path = notePath(id);
    if (fs::exists(current_path)) {
        snapshotNote(id, "restore-backup");
    }

    NoteRecord note = payload["note"].get<NoteRecord>();
    note.updated_at = nowIso8601();
    writeNoteFile(note);
    snapshotNote(id, "restore");
    return true;
}

void NotesService::snapshotNote(const std::string & id, const std::string & reason) const {
    const auto current_path = notePath(id);
    if (!fs::exists(current_path)) {
        return;
    }

    std::ifstream existing(current_path);
    nlohmann::json current_json;
    existing >> current_json;

    const auto history_dir = historyDirectory(id);
    fs::create_directories(history_dir);

    const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    const auto file_path = fs::path(history_dir) / (std::to_string(stamp) + "_" + slugify(reason) + ".json");

    nlohmann::json payload = {
        {"timestamp", nowIso8601()},
        {"reason", reason},
        {"note", current_json}
    };

    std::ofstream out(file_path.string(), std::ios::binary | std::ios::trunc);
    out << payload.dump(2);
}
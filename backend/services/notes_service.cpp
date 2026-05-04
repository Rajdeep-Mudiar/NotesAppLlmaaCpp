#include "services/notes_service.h"
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <unordered_map>

namespace fs = std::filesystem;

// Helper to sanitize path values from env (trim quotes/spaces)
std::string sanitizePathValue(std::string s) {
    if (s.empty()) return s;
    s.erase(0, s.find_first_not_of(" \t\r\n\"'"));
    s.erase(s.find_last_not_of(" \t\r\n\"'") + 1);
    return s;
}

// Helper to run shell commands and get output (capturing stderr too)
std::string execCommand(const std::string& cmd) {
    std::string result;
    // Redirect stderr to stdout so we see errors
    std::string full_cmd = cmd + " 2>&1";
    
#if defined(_WIN32)
    // On Windows, if the command has multiple quoted arguments, 
    // we must wrap the whole thing in another set of quotes for cmd /c
    std::string win_cmd = "\"" + full_cmd + "\"";
    std::array<char, 256> buffer;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(win_cmd.c_str(), "r"), _pclose);
#else
    std::array<char, 256> buffer;
    std::unique_ptr<FILE, decltype(&_pclose)> pipe(popen(full_cmd.c_str(), "r"), pclose);
#endif
    
    if (!pipe) {
        return "{\"error\": \"Failed to launch process\"}";
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

NotesService::NotesService(std::string base_directory, std::string executable_dir) 
    : base_directory_(std::move(base_directory)), executable_dir_(std::move(executable_dir)) {
    if (executable_dir_.empty()) {
        executable_dir_ = fs::current_path().string();
    }
}

NoteRecord NotesService::addNote(const std::string & title, const std::string & content, const std::vector<std::string> & tags) {
    nlohmann::json j;
    j["title"] = title.empty() ? "Untitled note" : title;
    j["content"] = content;
    j["tags"] = tags.empty() ? generateTags(j["title"], content) : normalizeTags(tags);
    j["created_at"] = nowIso8601();
    j["updated_at"] = j["created_at"];

    std::string temp_path = (fs::path(executable_dir_) / "temp_add.json").string();
    std::ofstream tmp(temp_path);
    tmp << j.dump();
    tmp.close();

    std::string python_cmd = "python";
    if (const char* env_p = std::getenv("SECOND_BRAIN_PYTHON")) {
        python_cmd = sanitizePathValue(env_p);
    }

    std::string bridge_path = (fs::path(executable_dir_) / "storage_bridge.py").string();
    std::string cmd = "\"" + python_cmd + "\" \"" + bridge_path + "\" save_file \"" + temp_path + "\"";
    std::string out = execCommand(cmd);
    
    if (out.empty()) {
        throw std::runtime_error("Empty output from storage bridge. Command: " + cmd);
    }

    try {
        // Find JSON boundaries in case of warnings in output
        size_t first = out.find('{');
        size_t last = out.find_last_of('}');
        if (first == std::string::npos || last == std::string::npos) {
             throw std::runtime_error("Invalid JSON format in bridge output: " + out);
        }
        std::string json_part = out.substr(first, last - first + 1);

        auto res = nlohmann::json::parse(json_part);
        if (res.contains("error")) throw std::runtime_error(res["error"]);
        
        NoteRecord record;
        record.id = res["id"];
        record.title = j["title"];
        record.content = j["content"];
        record.tags = j["tags"].get<std::vector<std::string>>();
        return record;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] addNote failed. \nCommand: " << cmd << "\nError: " << e.what() << "\nRaw Output: " << out << std::endl;
        throw;
    }
}

bool NotesService::updateNote(const std::string & id, const std::string & title, const std::string & content, const std::vector<std::string> & tags) {
    nlohmann::json j;
    j["id"] = id;
    j["title"] = title;
    j["content"] = content;
    j["tags"] = normalizeTags(tags);
    j["updated_at"] = nowIso8601();

    std::string temp_path = (fs::path(executable_dir_) / "temp_upd.json").string();
    std::ofstream tmp(temp_path);
    tmp << j.dump();
    tmp.close();

    std::string python_cmd = "python";
    if (const char* env_p = std::getenv("SECOND_BRAIN_PYTHON")) {
        python_cmd = sanitizePathValue(env_p);
    }

    std::string bridge_path = (fs::path(executable_dir_) / "storage_bridge.py").string();
    std::string cmd = "\"" + python_cmd + "\" \"" + bridge_path + "\" save_file \"" + temp_path + "\"";
    execCommand(cmd);
    return true;
}

bool NotesService::deleteNote(const std::string & id) {
    std::string python_cmd = "python";
    if (const char* env_p = std::getenv("SECOND_BRAIN_PYTHON")) {
        python_cmd = sanitizePathValue(env_p);
    }

    std::string bridge_path = (fs::path(executable_dir_) / "storage_bridge.py").string();
    std::cout << "[DEBUG] Deleting note: " << id << std::endl;
    execCommand("\"" + python_cmd + "\" \"" + bridge_path + "\" delete " + id);
    return true;
}

std::vector<NoteRecord> NotesService::loadNotes() const {
    std::string python_cmd = "python";
    if (const char* env_p = std::getenv("SECOND_BRAIN_PYTHON")) {
        python_cmd = sanitizePathValue(env_p);
    }

    std::string bridge_path = (fs::path(executable_dir_) / "storage_bridge.py").string();
    std::string cmd = "\"" + python_cmd + "\" \"" + bridge_path + "\" list";
    std::string out = execCommand(cmd);
    
    try {
        size_t first = out.find('[');
        size_t last = out.find_last_of(']');
        
        if (first == std::string::npos || last == std::string::npos) {
            if (out.find("error") != std::string::npos) {
                std::cerr << "[ERROR] MongoDB Bridge Error: " << out << std::endl;
            }
            return {};
        }

        std::string json_part = out.substr(first, last - first + 1);
        
        // Use ignore/replace for invalid UTF-8 bytes to prevent crashes
        auto j = nlohmann::json::parse(json_part, nullptr, false);
        if (j.is_discarded()) {
            std::cerr << "[ERROR] JSON Parse Discarded. Raw: " << out.substr(0, 100) << "..." << std::endl;
            return {};
        }

        std::vector<NoteRecord> notes;
        for (const auto& item : j) {
            try {
                notes.push_back(item.get<NoteRecord>());
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Failed to parse individual note: " << e.what() << std::endl;
            }
        }
        return notes;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] loadNotes failed: " << e.what() << std::endl;
        return {};
    }
}

std::vector<NoteRecord> NotesService::searchNotes(const std::string & query, std::size_t limit) const {
    auto notes = loadNotes();
    auto query_tokens = tokenize(toLower(query));

    std::vector<std::pair<double, NoteRecord>> scored;
    for (const auto & note : notes) {
        double score = relevanceScore(note, query_tokens);
        if (score > 0) {
            scored.push_back({score, note});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const auto & a, const auto & b) {
        return a.first > b.first;
    });

    std::vector<NoteRecord> results;
    for (std::size_t i = 0; i < std::min(limit, scored.size()); ++i) {
        results.push_back(scored[i].second);
    }
    return results;
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

std::string NotesService::nowIso8601() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&in_time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
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
        if (!tag.empty() && std::find(result.begin(), result.end(), tag) == result.end()) {
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
            tokens.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::vector<std::string> NotesService::generateTags(const std::string & title, const std::string & content) {
    std::unordered_map<std::string, int> counts;
    for (const auto & token : tokenize(title + " " + content)) {
        if (token.size() >= 4) ++counts[token];
    }

    std::vector<std::pair<std::string, int>> ranking(counts.begin(), counts.end());
    std::sort(ranking.begin(), ranking.end(), [](const auto & lhs, const auto & rhs) {
        return lhs.second > rhs.second;
    });

    std::vector<std::string> tags;
    for (const auto & [token, count] : ranking) {
        tags.push_back(token);
        if (tags.size() == 5) break;
    }
    if (tags.empty()) tags.push_back("general");
    return tags;
}

double NotesService::relevanceScore(const NoteRecord & note, const std::vector<std::string> & query_tokens) {
    if (query_tokens.empty()) return 1.0;
    
    std::string title_low = toLower(note.title);
    std::string content_low = toLower(note.content);
    const auto note_tokens = tokenize(title_low + " " + content_low);
    
    double score = 0.0;
    
    // Check for exact phrase matches in title (HUGE BOOST)
    std::string full_query;
    for (const auto& t : query_tokens) full_query += (full_query.empty() ? "" : " ") + t;
    if (title_low.find(full_query) != std::string::npos) score += 50.0;

    for (const auto & token : query_tokens) {
        // Title match boost
        if (title_low.find(token) != std::string::npos) score += 10.0;
        
        // Tag match boost
        for (const auto& tag : note.tags) {
            if (toLower(tag).find(token) != std::string::npos) score += 8.0;
        }

        // Frequency match (normalize by note length to avoid favoring huge notes)
        int count = std::count(note_tokens.begin(), note_tokens.end(), token);
        score += (count * 1.0);
    }
    
    return score;
}

// Stubs for history which we don't need for MongoDB simple version
nlohmann::json NotesService::noteHistory(const std::string & id) const { return nlohmann::json::array(); }
bool NotesService::restoreVersion(const std::string & id, const std::string & version_file) { return false; }
std::string NotesService::notePath(const std::string & id) const { return ""; }
std::string NotesService::historyDirectory(const std::string & id) const { return ""; }
void NotesService::snapshotNote(const std::string & id, const std::string & reason) const {}
void NotesService::writeNoteFile(const NoteRecord & note) const {}
NoteRecord NotesService::readNoteFile(const std::string & path) const { return {}; }
std::string NotesService::slugify(const std::string & value) { return value; }
std::string NotesService::newId() { return ""; }
std::string NotesService::baseDirectory() const { return base_directory_; }

void to_json(nlohmann::json & j, const NoteRecord & n) {
    j = nlohmann::json{
        {"id", n.id},
        {"title", n.title},
        {"content", n.content},
        {"tags", n.tags},
        {"created_at", n.created_at},
        {"updated_at", n.updated_at}
    };
}

void from_json(const nlohmann::json & j, NoteRecord & n) {
    n.id = j.value("id", "");
    n.title = j.value("title", "");
    n.content = j.value("content", "");
    n.tags = j.value("tags", std::vector<std::string>{});
    n.created_at = j.value("created_at", "");
    n.updated_at = j.value("updated_at", "");
}

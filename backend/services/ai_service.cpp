#include "ai_service.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <unordered_map>

namespace {
bool containsNegation(const std::string & text) {
    const auto lower = text;
    return lower.find(" not ") != std::string::npos || lower.find(" never ") != std::string::npos || lower.find(" no ") != std::string::npos || lower.find("without") != std::string::npos;
}
} // namespace

AiService::AiService(NotesService & notes_service, AiEngine & ai_engine)
    : notes_service_(notes_service), ai_engine_(ai_engine) {
}

std::string AiService::summarize(const std::string & content) {
    if (content.size() <= 220) {
        return content;
    }
    return content.substr(0, 217) + "...";
}

std::vector<std::string> AiService::splitSentences(const std::string & content) {
    std::vector<std::string> sentences;
    std::string current;
    for (char ch : content) {
        current.push_back(ch);
        if (ch == '.' || ch == '!' || ch == '?') {
            std::string trimmed = current;
            trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c) { return !std::isspace(c); }));
            trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), trimmed.end());
            if (!trimmed.empty()) {
                sentences.push_back(trimmed);
            }
            current.clear();
        }
    }
    if (!current.empty()) {
        sentences.push_back(current);
    }
    return sentences;
}

std::vector<std::string> AiService::extractConcepts(const NoteRecord & note) {
    std::vector<std::string> concepts = note.tags;
    std::stringstream stream(note.title + " " + note.content);
    std::string word;
    while (stream >> word) {
        word.erase(std::remove_if(word.begin(), word.end(), [](unsigned char ch) { return !std::isalnum(ch); }), word.end());
        std::transform(word.begin(), word.end(), word.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (word.size() >= 5 && std::find(concepts.begin(), concepts.end(), word) == concepts.end()) {
            concepts.push_back(word);
        }
        if (concepts.size() >= 8) {
            break;
        }
    }
    return concepts;
}

std::string AiService::joinList(const std::vector<std::string> & items, const std::string & delimiter) {
    std::ostringstream out;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i != 0) {
            out << delimiter;
        }
        out << items[i];
    }
    return out.str();
}

std::string AiService::modeInstruction(const std::string & mode, const std::string & persona) {
    if (mode == "argument") {
        return "Debate the user's position using only the notes. Be sharp but respectful.";
    }
    if (persona == "critic") {
        return "Respond like a critical reviewer who tests assumptions and points out weak evidence.";
    }
    if (persona == "examiner") {
        return "Respond like an examiner. Ask precise questions and judge the completeness of the notes.";
    }
    return "Respond like a clear, supportive teacher.";
}

std::string AiService::fallbackAnswer(const std::string & query, const std::vector<NoteRecord> & notes) {
    if (notes.empty()) {
        return "I could not find any notes related to: " + query + ".";
    }

    std::ostringstream out;
    out << "I found " << notes.size() << " note(s) related to your question. Key points: ";
    for (std::size_t i = 0; i < notes.size(); ++i) {
        if (i != 0) {
            out << " | ";
        }
        out << notes[i].title << ": " << summarize(notes[i].content);
    }
    return out.str();
}

nlohmann::json AiService::buildSearchResponse(const std::string & query, const std::string & mode, const std::string & persona) const {
    const auto relevant_notes = notes_service_.searchNotes(query, 4);

    std::ostringstream context;
    for (const auto & note : relevant_notes) {
        context << "[Title] " << note.title << "\n";
        context << "[Tags] " << joinList(note.tags, ", ") << "\n";
        context << "[Content] " << note.content << "\n\n";
    }

    const std::string prompt =
        "You are an AI Second Brain assistant. Answer ONLY from the provided notes. "
        "If the notes do not contain enough information, say what is missing. Do not invent facts.\n\n"
        + modeInstruction(mode, persona) + "\n\n"
        "User question: " + query + "\n\n"
        "Notes:\n" + context.str() + "\n"
        "Answer in a concise, helpful way and cite note titles when relevant.";

    std::string answer = ai_engine_.generate(prompt, 256);
    if (answer.empty()) {
        answer = fallbackAnswer(query, relevant_notes);
    }

    nlohmann::json response;
    response["answer"] = answer;
    response["mode"] = mode;
    response["persona"] = persona;
    response["query"] = query;
    response["relevant_notes"] = nlohmann::json::array();

    for (const auto & note : relevant_notes) {
        response["relevant_notes"].push_back({
            {"id", note.id},
            {"title", note.title},
            {"content", note.content},
            {"tags", note.tags},
            {"summary", summarize(note.content)}
        });
    }

    response["flashcards"] = buildFlashcards()["flashcards"];
    response["graph"] = buildGraph();
    response["contradictions"] = buildContradictions()["contradictions"];
    response["learning_path"] = buildLearningPath()["learning_path"];
    response["self_questions"] = buildSelfQuestions()["self_questions"];
    response["ideas"] = buildIdeas()["ideas"];
    response["insights"] = buildInsights();
    return response;
}

nlohmann::json AiService::buildInsights() const {
    const auto notes = notes_service_.loadNotes();
    std::set<std::string> tag_set;
    std::size_t total_tags = 0;
    for (const auto & note : notes) {
        total_tags += note.tags.size();
        tag_set.insert(note.tags.begin(), note.tags.end());
    }

    nlohmann::json insights;
    insights["note_count"] = notes.size();
    insights["unique_tags"] = tag_set.size();
    insights["tag_density"] = notes.empty() ? 0.0 : static_cast<double>(total_tags) / static_cast<double>(notes.size());
    insights["recent_notes"] = nlohmann::json::array();
    for (std::size_t i = 0; i < std::min<std::size_t>(notes.size(), 5); ++i) {
        insights["recent_notes"].push_back({
            {"id", notes[i].id},
            {"title", notes[i].title},
            {"summary", summarize(notes[i].content)},
            {"tags", notes[i].tags}
        });
    }
    return insights;
}

nlohmann::json AiService::buildFlashcards() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json flashcards = nlohmann::json::array();
    for (const auto & note : notes) {
        const auto sentences = splitSentences(note.content);
        const std::string answer = sentences.empty() ? summarize(note.content) : sentences.front();
        flashcards.push_back({
            {"id", note.id},
            {"front", "What is the main idea of \"" + note.title + "\"?"},
            {"back", answer},
            {"tags", note.tags}
        });
    }

    return {{"flashcards", flashcards}};
}

nlohmann::json AiService::buildGraph() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json nodes = nlohmann::json::array();
    nlohmann::json edges = nlohmann::json::array();
    std::unordered_map<std::string, bool> concept_ids;

    for (const auto & note : notes) {
        nodes.push_back(nlohmann::json{{"id", note.id}, {"label", note.title}, {"type", "note"}, {"tags", note.tags}});
        const auto concepts = extractConcepts(note);
        for (const auto & concept_label : concepts) {
            const std::string concept_id = "concept:" + concept_label;
            if (!concept_ids.contains(concept_id)) {
                concept_ids[concept_id] = true;
                nodes.push_back(nlohmann::json{{"id", concept_id}, {"label", concept_label}, {"type", "concept"}});
            }
            edges.push_back(nlohmann::json{{"source", note.id}, {"target", concept_id}, {"kind", "mentions"}});
        }
    }

    for (std::size_t i = 0; i < notes.size(); ++i) {
        for (std::size_t j = i + 1; j < notes.size(); ++j) {
            std::vector<std::string> shared;
            for (const auto & tag : notes[i].tags) {
                if (std::find(notes[j].tags.begin(), notes[j].tags.end(), tag) != notes[j].tags.end()) {
                    shared.push_back(tag);
                }
            }
            if (!shared.empty()) {
                edges.push_back(nlohmann::json{{"source", notes[i].id}, {"target", notes[j].id}, {"kind", "shared-tag"}, {"labels", shared}});
            }
        }
    }

    return {{"nodes", nodes}, {"edges", edges}};
}

nlohmann::json AiService::buildContradictions() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json contradictions = nlohmann::json::array();

    for (std::size_t i = 0; i < notes.size(); ++i) {
        for (std::size_t j = i + 1; j < notes.size(); ++j) {
            std::vector<std::string> shared;
            for (const auto & tag : notes[i].tags) {
                if (std::find(notes[j].tags.begin(), notes[j].tags.end(), tag) != notes[j].tags.end()) {
                    shared.push_back(tag);
                }
            }
            if (shared.empty()) {
                continue;
            }

            const bool left_neg = containsNegation(notes[i].content);
            const bool right_neg = containsNegation(notes[j].content);
            if (left_neg != right_neg) {
                contradictions.push_back({
                    {"left", notes[i].title},
                    {"right", notes[j].title},
                    {"shared_tags", shared},
                    {"severity", "medium"},
                    {"summary", "These notes present opposite polarity on a shared concept."}
                });
            }
        }
    }

    return {{"contradictions", contradictions}};
}

nlohmann::json AiService::buildLearningPath() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json path = nlohmann::json::array();
    for (const auto & note : notes) {
        path.push_back({
            {"step", path.size() + 1},
            {"title", "Review note: " + note.title},
            {"reason", "This note connects to " + joinList(note.tags, ", ") + "."}
        });
    }
    return {{"learning_path", path}};
}

nlohmann::json AiService::buildSelfQuestions() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json questions = nlohmann::json::array();
    for (const auto & note : notes) {
        const auto concepts = extractConcepts(note);
        if (!concepts.empty()) {
            questions.push_back({{"question", "How does " + concepts.front() + " relate to " + note.title + "?"}});
        }
        questions.push_back({{"question", "What evidence in \"" + note.title + "\" supports its key claim?"}});
    }
    return {{"self_questions", questions}};
}

nlohmann::json AiService::buildIdeas() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json ideas = nlohmann::json::array();
    for (const auto & note : notes) {
        ideas.push_back({
            {"title", "Turn \"" + note.title + "\" into a project"},
            {"idea", "Combine " + joinList(note.tags, ", ") + " into a concrete experiment or checklist."}
        });
    }
    return {{"ideas", ideas}};
}
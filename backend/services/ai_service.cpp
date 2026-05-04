#include "ai_service.h"

#include <algorithm>
#include <cctype>
#include <iostream>
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
        return "I couldn't find any information about '" + query + "' in your notes.";
    }

    std::ostringstream out;
    out << "I couldn't generate a direct AI answer, but I found relevant info in these notes: ";
    for (std::size_t i = 0; i < notes.size(); ++i) {
        if (i != 0) out << ", ";
        out << notes[i].title;
    }
    return out.str();
}

nlohmann::json AiService::buildSearchResponse(const std::string & query, const std::string & mode, const std::string & persona) const {
    std::cout << "[AI] Searching for: \"" << query << "\"" << std::endl;
    // Reduce from 8 to 4 to stay within context limits
    const auto relevant_notes = notes_service_.searchNotes(query, 4);
    std::cout << "[AI] Found " << relevant_notes.size() << " relevant notes." << std::endl;

    std::ostringstream context;
    for (const auto & note : relevant_notes) {
        context << "[Title] " << note.title << "\n";
        context << "[Tags] " << joinList(note.tags, ", ") << "\n";
        context << "[Content] " << note.content << "\n\n";
    }

    const std::string prompt =
        "System: You are an AI Second Brain. Use the provided notes to answer. If unsure, say you don't know.\n\n"
        "Context (Your Notes):\n" + context.str() + "\n"
        "Style: " + modeInstruction(mode, persona) + "\n\n"
        "User: " + query + "\n"
        "Assistant: @@@ANSWER_START@@@\n";

    std::cout << "[AI] Generating answer..." << std::endl;
    std::string answer = ai_engine_.generate(prompt, 512);
    if (answer.empty()) {
        std::cout << "[AI] Generation failed, using fallback." << std::endl;
        answer = fallbackAnswer(query, relevant_notes);
    } else {
        std::cout << "[AI] Answer generated successfully." << std::endl;
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

nlohmann::json AiService::buildFlashcards(int count) const {
    auto notes = notes_service_.loadNotes();
    if (notes.empty()) {
        return {{"flashcards", nlohmann::json::array()}};
    }

    // Limit to 10 most recent notes for flashcard generation context
    if (notes.size() > 10) {
        notes.resize(10);
    }

    std::ostringstream context;
    for (const auto & note : notes) {
        context << "Title: " << note.title << "\nContent: " << note.content << "\n---\n";
    }

    const std::string prompt =
        "You are an expert tutor. Based on the notes below, generate exactly " + std::to_string(count) + " flashcards.\n"
        "Rules:\n"
        "1. Each flashcard must have a 'front' (question) and a 'back' (detailed answer).\n"
        "2. The questions should be challenging and focus on key concepts from the notes.\n"
        "3. Output ONLY a JSON array of objects. Format: [{\"front\": \"...\", \"back\": \"...\"}, ...]\n\n"
        "Notes:\n" + context.str() + "\n"
        "JSON Flashcards:\n"
        "@@@JSON_START@@@\n";

    std::string response = ai_engine_.generate(prompt, 1024); // Increased to 1024 for more cards
    nlohmann::json flashcards = nlohmann::json::array();
    try {
        // Try to find the JSON array in the response
        auto start = response.find('[');
        auto end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string json_str = response.substr(start, end - start + 1);
            flashcards = nlohmann::json::parse(json_str);
        } else {
            // Fallback: simple split if AI fails to give JSON
            std::cerr << "[WARN] Flashcard generation did not return valid JSON, using simple fallback." << std::endl;
        }
    } catch (...) {
        std::cerr << "[ERROR] Failed to parse flashcards JSON." << std::endl;
    }

    // If AI failed or returned empty, do a basic fallback
    if (flashcards.empty() || !flashcards.is_array()) {
        flashcards = nlohmann::json::array();
        for (std::size_t i = 0; i < std::min<std::size_t>(notes.size(), static_cast<std::size_t>(count)); ++i) {
            const auto sentences = splitSentences(notes[i].content);
            flashcards.push_back({
                {"front", "Question about " + notes[i].title},
                {"back", sentences.empty() ? notes[i].content : sentences.front()}
            });
        }
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
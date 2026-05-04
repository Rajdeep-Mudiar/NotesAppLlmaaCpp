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
    std::string instruction = "Respond as a helpful AI assistant. ";
    
    if (persona == "critic") {
        instruction = "Respond like a sharp, critical reviewer. Challenge the ideas in the notes, find contradictions, and point out missing evidence. ";
    } else if (persona == "examiner") {
        instruction = "Respond like a strict examiner. Ask the user deep, probing questions about the concepts in their notes to test their understanding. ";
    } else if (persona == "student") {
        instruction = "Respond like a curious student. Ask for clarifications and try to summarize the concepts simply. ";
    } else {
        instruction = "Respond like a supportive, expert teacher. Explain concepts clearly and relate them to practical examples. ";
    }

    if (mode == "summarize") {
        instruction += "Provide a structured, bulleted summary of the key points.";
    } else if (mode == "critique") {
        instruction += "Critique the provided notes and suggest improvements.";
    } else {
        instruction += "Use the notes to answer the user's question, but feel free to use your own 'internal brain' to fill in gaps or provide broader context.";
    }
    
    return instruction;
}

std::string AiService::fallbackAnswer(const std::string & query, const std::vector<NoteRecord> & notes) {
    if (notes.empty()) {
        return "I don't have any specific notes on '" + query + "', but based on my general knowledge: " + query + " is often related to basic concepts in this field. Try adding some notes about it!";
    }

    std::ostringstream out;
    out << "I'm having trouble synthesizing a deep answer right now, but your notes on '" << notes[0].title << "' suggest that this involves " << summarize(notes[0].content);
    return out.str();
}

nlohmann::json AiService::buildSearchResponse(const std::string & query, const std::string & mode, const std::string & persona) const {
    std::cout << "[AI] Query: \"" << query << "\"" << std::endl;
    const auto relevant_notes = notes_service_.searchNotes(query, 4);
    
    std::ostringstream context;
    if (relevant_notes.empty()) {
        context << "(No notes found for this specific query.)";
    } else {
        for (const auto & note : relevant_notes) {
            // TRUNCATE content to avoid context overflow
            std::string snippet = note.content;
            if (snippet.size() > 1200) {
                snippet = snippet.substr(0, 1197) + "...";
            }
            context << "[Note: " << note.title << "]\n" << snippet << "\n\n";
        }
    }

    const std::string prompt =
        "<|im_start|>system\n"
        "You are the 'Second Brain AI'. Answer the user's question using the provided notes.\n"
        "If you use your general knowledge, clarify that it's from your 'brain' and not the notes.\n"
        "Personality: " + modeInstruction(mode, persona) + "\n"
        "Context (Notes):\n" + context.str() + "<|im_end|>\n"
        "<|im_start|>user\n"
        "Question: " + query + "<|im_end|>\n"
        "<|im_start|>assistant\n"
        "@@@ANSWER_START@@@\n";

    std::string answer = ai_engine_.generate(prompt, 600);
    if (answer.empty()) {
        if (relevant_notes.empty()) {
            answer = "I don't have any notes on that. Based on my general knowledge, " + query + " usually refers to...";
        } else {
            std::ostringstream fall;
            fall << "I'm having a brief connection issue with my synthesis core, but I found relevant info in these notes: ";
            for (size_t i = 0; i < relevant_notes.size(); ++i) {
                if (i > 0) fall << ", ";
                fall << "'" << relevant_notes[i].title << "'";
            }
            fall << ". Please check those notes directly for the answer.";
            answer = fall.str();
        }
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
            {"summary", summarize(note.content)},
            {"tags", note.tags}
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

    // Use most recent 5 notes
    if (notes.size() > 5) notes.resize(5);

    std::ostringstream context;
    for (const auto & note : notes) {
        context << "Title: " << note.title << "\nContent: " << note.content << "\n---\n";
    }

    const std::string prompt =
        "<|im_start|>system\n"
        "You are a study assistant. Generate exactly " + std::to_string(count) + " flashcards based on the notes below.\n"
        "Format: Return ONLY a valid JSON array of objects with 'front' and 'back' fields.\n"
        "Example: [{\"front\": \"Question?\", \"back\": \"Answer.\"}]\n"
        "Notes:\n" + context.str() + "<|im_end|>\n"
        "<|im_start|>assistant\n"
        "@@@JSON_START@@@\n";

    std::string response = ai_engine_.generate(prompt, 1024);
    nlohmann::json flashcards = nlohmann::json::array();
    
    try {
        auto start = response.find('[');
        auto end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string json_str = response.substr(start, end - start + 1);
            flashcards = nlohmann::json::parse(json_str);
        }
    } catch (...) {
        std::cerr << "[ERROR] Flashcard JSON parse failed." << std::endl;
    }

    if (flashcards.empty() || !flashcards.is_array()) {
        flashcards = nlohmann::json::array();
        for (std::size_t i = 0; i < std::min<std::size_t>(notes.size(), (std::size_t)count); ++i) {
            flashcards.push_back({
                {"front", "Key concept in " + notes[i].title},
                {"back", summarize(notes[i].content)}
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
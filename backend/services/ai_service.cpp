#include "ai_service.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>

using json = nlohmann::json;

namespace {
bool containsNegation(const std::string & text) {
    const auto lower = text;
    return lower.find(" not ") != std::string::npos || lower.find(" never ") != std::string::npos || lower.find(" no ") != std::string::npos || lower.find("without") != std::string::npos;
}
std::vector<std::string> tokenize(const std::string & text) {
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
    if (!current.empty()) tokens.push_back(current);
    return tokens;
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
        "<|system|>\n"
        "You are the 'Second Brain AI'. Answer the user's question using the provided notes.\n"
        "If you use your general knowledge, clarify that it's from your 'brain' and not the notes.\n"
        "Personality: " + modeInstruction(mode, persona) + "\n"
        "Context (Notes):\n" + context.str() + "</s>\n"
        "<|user|>\n"
        "Question: " + query + "</s>\n"
        "<|assistant|>\n"
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

nlohmann::json AiService::buildFlashcards(int count, const std::string & difficulty, const std::vector<std::string> & noteIds) const {
    auto all_notes = notes_service_.loadNotes();
    std::vector<NoteRecord> filtered_notes;

    if (noteIds.empty()) {
        filtered_notes = all_notes;
    } else {
        for (const auto & id : noteIds) {
            for (const auto & note : all_notes) {
                if (note.id == id) {
                    filtered_notes.push_back(note);
                    break;
                }
            }
        }
    }

    if (filtered_notes.empty()) {
        return {{"flashcards", nlohmann::json::array()}};
    }

    std::ostringstream context;
    int char_count = 0;
    for (const auto & note : filtered_notes) {
        std::string entry = "SOURCE_NOTE: " + note.title + "\nTEXT_CONTENT: " + note.content + "\n---\n";
        if (char_count + entry.size() > 3000) break; 
        context << entry;
        char_count += (int)entry.size();
    }

    const std::string prompt =
        "<|system|>\n"
        "You are an Information Architect. Extract " + std::to_string(count) + " KEY CONCEPTS from the NOTES.\n"
        "GOAL: Provide information cards, NOT questions/answers.\n"
        "REQUIREMENTS:\n"
        "1. 'front' MUST be the NAME of a concept or topic from the notes (1-3 words).\n"
        "2. 'back' MUST be a concise SUMMARY of the most important info about that topic.\n"
        "3. DO NOT use question marks. ONLY provide facts.\n"
        "EXAMPLES:\n"
        "- front: \"Neural Networks\", back: \"Computational models inspired by the human brain, used to recognize patterns and solve complex problems.\"\n"
        "- front: \"Deep Learning\", back: \"A subset of ML that uses multi-layered neural networks for high-level abstraction.\"\n"
        "NOTES:\n" + context.str() + "</s>\n"
        "<|user|>\n"
        "Create " + std::to_string(count) + " info-cards in JSON format: [{\"front\": \"...\", \"back\": \"...\"}]</s>\n"
        "<|assistant|>\n"
        "["; 

    std::string response = ai_engine_.generate(prompt, 1024);
    if (response.find('[') != 0) response = "[" + response; // Ensure it starts with [
    
    nlohmann::json flashcards = nlohmann::json::array();
    
    try {
        auto start = response.find('[');
        auto end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end >= start) {
            std::string json_str = response.substr(start, end - start + 1);
            auto parsed = nlohmann::json::parse(json_str, nullptr, false);
            if (!parsed.is_discarded() && parsed.is_array()) {
                flashcards = parsed;
            }
        }
    } catch (...) {}

    if (flashcards.is_discarded() || !flashcards.is_array()) {
        flashcards = nlohmann::json::array();
    }

    if (flashcards.empty() || flashcards.size() < (std::size_t)count) {
        std::vector<std::string> sentences;
        for (const auto& note : filtered_notes) {
            auto s = splitSentences(note.content);
            for (auto& sent : s) {
                if (sent.size() > 30) sentences.push_back(sent);
            }
        }

        while (flashcards.size() < (std::size_t)count && !sentences.empty()) {
            std::size_t idx = flashcards.size() % sentences.size();
            std::string front = "Explain this concept from your notes: " + filtered_notes[0].title;
            if (sentences[idx].size() > 60) {
                 front = "What does this note mention about " + filtered_notes[0].title + "?";
            }
            
            flashcards.push_back({
                {"front", front},
                {"back", sentences[idx]}
            });
            if (flashcards.size() >= (std::size_t)count) break;
        }
    }

    return {{"flashcards", flashcards}};
}

nlohmann::json AiService::buildQuiz(int count, const std::string & difficulty, const std::vector<std::string> & noteIds) const {
    auto all_notes = notes_service_.loadNotes();
    std::vector<NoteRecord> filtered_notes;

    if (noteIds.empty()) {
        filtered_notes = all_notes;
    } else {
        for (const auto & id : noteIds) {
            for (const auto & note : all_notes) {
                if (note.id == id) {
                    filtered_notes.push_back(note);
                    break;
                }
            }
        }
    }

    if (filtered_notes.empty()) {
        return {{"questions", nlohmann::json::array()}};
    }

    std::ostringstream context;
    int char_count = 0;
    for (const auto & note : filtered_notes) {
        std::string entry = "SOURCE_NOTE: " + note.title + "\nTEXT_CONTENT: " + note.content + "\n---\n";
        if (char_count + entry.size() > 3000) break;
        context << entry;
        char_count += (int)entry.size();
    }

    const std::string prompt =
        "<|system|>\n"
        "You are a Professional Quiz Creator. Generate EXACTLY " + std::to_string(count) + " unique Multiple Choice Questions from the SOURCE_NOTES.\n"
        "STRICT DIVERSITY RULES:\n"
        "1. NO REPETITION: Every question must cover a different aspect or topic.\n"
        "2. UNIQUE OPTIONS: Do NOT use the same distractors for multiple questions.\n"
        "3. GROUNDING: Strictly use the provided notes.\n"
        "SOURCE_NOTES:\n" + context.str() + "\n"
        "JSON FORMAT:</s>\n"
        "<|user|>\n"
        "Generate EXACTLY " + std::to_string(count) + " different MCQs now.</s>\n"
        "<|assistant|>\n"
        "[";

    std::string response = ai_engine_.generate(prompt, 2048);
    
    nlohmann::json questions = nlohmann::json::array();
    try {
        auto start = response.find('[');
        auto end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end > start) {
            std::string json_str = response.substr(start, end - start + 1);
            auto parsed = nlohmann::json::parse(json_str, nullptr, false);
            if (!parsed.is_discarded() && parsed.is_array()) {
                questions = parsed;
            }
        }
    } catch (...) {}

    // DYNAMIC FALLBACK: Diverse questions and rotating options
    if (questions.empty() || (int)questions.size() < count) {
        std::vector<std::string> distractor_pool = {
            "A systematic methodology for architectural design.",
            "The primary protocol for high-level data integration.",
            "A theoretical framework for optimized performance.",
            "The core strategy for resource allocation.",
            "A comparative analysis of implementation models.",
            "The foundational principle of systemic scaling.",
            "A comprehensive overview of functional requirements.",
            "The standardized approach to modular development."
        };

        for (int i = (int)questions.size(); i < count; ++i) {
            const auto & note = filtered_notes[i % filtered_notes.size()];
            nlohmann::json q = nlohmann::json::object();
            
            std::string topic = !note.tags.empty() ? note.tags[i % note.tags.size()] : note.title;
            
            // Vary the question style
            if (i % 2 == 0) {
                q["question"] = "Which of the following is the defining characteristic of '" + topic + "' according to the notes?";
            } else {
                q["question"] = "Based on the material in '" + note.title + "', what role does " + topic + " play in the overall system?";
            }
            
            nlohmann::json opts = nlohmann::json::array();
            opts.push_back("The optimized execution of " + topic + " based on note criteria."); // Correct
            
            // Pick 3 unique distractors from the pool based on index
            std::set<int> picked_indices;
            while (picked_indices.size() < 3) {
                int idx = (i * 3 + (int)picked_indices.size()) % distractor_pool.size();
                picked_indices.insert(idx);
            }
            
            for (int idx : picked_indices) {
                opts.push_back(distractor_pool[idx]);
            }
            
            q["options"] = opts;
            q["answer"] = opts[0];
            questions.push_back(q);
        }
    }

    return {{"questions", questions}};
}

nlohmann::json AiService::buildGraph() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json nodes = nlohmann::json::array();
    nlohmann::json edges = nlohmann::json::array();
    std::unordered_map<std::string, bool> concept_ids;

    for (const auto & note : notes) {
        json n = json::object();
        n["id"] = note.id;
        n["label"] = note.title;
        n["type"] = "note";
        n["tags"] = note.tags;
        nodes.push_back(n);
        const auto concepts = extractConcepts(note);
        for (const auto & concept_label : concepts) {
            const std::string concept_id = "concept:" + concept_label;
            if (!concept_ids.contains(concept_id)) {
                concept_ids[concept_id] = true;
                json c = json::object();
                c["id"] = concept_id;
                c["label"] = concept_label;
                c["type"] = "concept";
                nodes.push_back(c);
            }
            json e = json::object();
            e["source"] = note.id;
            e["target"] = concept_id;
            e["kind"] = "mentions";
            edges.push_back(e);
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
                json e = json::object();
                e["source"] = notes[i].id;
                e["target"] = notes[j].id;
                e["kind"] = "shared-tag";
                e["labels"] = shared;
                edges.push_back(e);
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
                nlohmann::json c = nlohmann::json::object();
                c["left"] = notes[i].title;
                c["right"] = notes[j].title;
                c["shared_tags"] = shared;
                c["severity"] = "medium";
                c["summary"] = "These notes present opposite polarity on a shared concept.";
                contradictions.push_back(c);
            }
        }
    }

    return {{"contradictions", contradictions}};
}

nlohmann::json AiService::buildLearningPath() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json path = nlohmann::json::array();
    for (const auto & note : notes) {
        nlohmann::json s = nlohmann::json::object();
        s["step"] = (int)(path.size() + 1);
        s["title"] = "Review note: " + note.title;
        s["reason"] = "This note connects to " + joinList(note.tags, ", ") + ".";
        path.push_back(s);
    }
    return {{"learning_path", path}};
}

nlohmann::json AiService::buildSelfQuestions() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json questions = nlohmann::json::array();
    for (const auto & note : notes) {
        const auto concepts = extractConcepts(note);
        if (!concepts.empty()) {
            nlohmann::json q1 = nlohmann::json::object();
            q1["question"] = "How does " + concepts.front() + " relate to " + note.title + "?";
            questions.push_back(q1);
        }
        nlohmann::json q2 = nlohmann::json::object();
        q2["question"] = "What evidence in \"" + note.title + "\" supports its key claim?";
        questions.push_back(q2);
    }
    return {{"self_questions", questions}};
}

nlohmann::json AiService::buildIdeas() const {
    const auto notes = notes_service_.loadNotes();
    nlohmann::json ideas = nlohmann::json::array();
    for (const auto & note : notes) {
        nlohmann::json id = nlohmann::json::object();
        id["title"] = "Turn \"" + note.title + "\" into a project";
        id["idea"] = "Combine " + joinList(note.tags, ", ") + " into a concrete experiment or checklist.";
        ideas.push_back(id);
    }
    return {{"ideas", ideas}};
}
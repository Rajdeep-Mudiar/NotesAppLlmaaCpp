#include "ai_service.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <map>
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
        "1. NO REPETITION: Every single question MUST cover a completely different aspect or sub-topic.\n"
        "2. UNIQUE OPTIONS: Do NOT reuse the same distractors across different questions.\n"
        "3. CONTEXTUAL BOUNDARIES: Read the SOURCE_NOTES to understand the core subject. You may synthesize your own high-quality questions using your broader knowledge about these subjects, but DO NOT go out of context. Stay strictly within the domains discussed in the notes.\n"
        "SOURCE_NOTES:\n" + context.str() + "\n"
        "JSON FORMAT: Provide an array of objects. Each object must have 'question', 'options' (array of 4 distinct strings), and 'answer' (exact match to one option).\n"
        "EXAMPLE (Do not copy this content, use the notes):\n"
        "[{\"question\":\"What is X?\", \"options\":[\"A\",\"B\",\"C\",\"D\"], \"answer\":\"A\"}]\n"
        "</s>\n"
        "<|user|>\n"
        "Generate EXACTLY " + std::to_string(count) + " different MCQs now. ONLY output the raw JSON array.</s>\n"
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
        std::vector<std::string> distractor_pool;
        for (const auto& note : filtered_notes) {
            auto s = splitSentences(note.content);
            for (auto& sent : s) {
                if (sent.size() > 20 && sent.size() < 120) distractor_pool.push_back(sent);
            }
        }
        
        // Add generic fallbacks only if we don't have enough sentences from notes
        if (distractor_pool.size() < 4) {
            distractor_pool.push_back("A systematic methodology for architectural design.");
            distractor_pool.push_back("The primary protocol for high-level data integration.");
            distractor_pool.push_back("A theoretical framework for optimized performance.");
            distractor_pool.push_back("The core strategy for resource allocation.");
            distractor_pool.push_back("A comparative analysis of implementation models.");
            distractor_pool.push_back("The foundational principle of systemic scaling.");
        }

        std::vector<std::string> word_pool;
        for (const auto& s : distractor_pool) {
            std::istringstream iss(s);
            std::string w;
            while(iss >> w) {
                std::string clean;
                for (char c : w) if (!std::ispunct(c)) clean += c;
                if (clean.size() > 5) word_pool.push_back(clean);
            }
        }
        if (word_pool.size() < 4) {
            word_pool.push_back("Algorithm"); word_pool.push_back("Structure");
            word_pool.push_back("Process"); word_pool.push_back("System");
        }

        for (int i = (int)questions.size(); i < count; ++i) {
            nlohmann::json q = nlohmann::json::object();
            
            int correct_idx = (i * 5 + 1) % distractor_pool.size();
            std::string sentence = distractor_pool[correct_idx];
            
            std::istringstream iss(sentence);
            std::string word, target_word;
            while(iss >> word) {
                std::string clean;
                for (char c : word) if (!std::ispunct(c)) clean += c;
                if (clean.size() > 5) {
                    target_word = clean;
                    break;
                }
            }
            
            if (target_word.empty()) target_word = "concept";
            
            std::string question_text = sentence;
            size_t pos = question_text.find(target_word);
            if (pos != std::string::npos) {
                question_text.replace(pos, target_word.length(), "______");
            } else {
                question_text += " (What is the key term?)";
            }
            
            q["question"] = "Fill in the blank: " + question_text;
            std::string correct_answer = target_word;
            
            std::set<std::string> picked_words;
            picked_words.insert(correct_answer);
            int attempt = 0;
            while (picked_words.size() < 4 && attempt < 50) {
                int w_idx = (i * 7 + (int)picked_words.size() * 13 + attempt) % word_pool.size();
                picked_words.insert(word_pool[w_idx]);
                attempt++;
            }
            
            std::vector<std::string> options_vec;
            for (const auto& w : picked_words) {
                if (w != correct_answer) options_vec.push_back(w);
            }
            if (options_vec.size() > 3) options_vec.resize(3);
            
            int correct_pos = (i * 17) % 4;
            if (correct_pos > (int)options_vec.size()) correct_pos = options_vec.size();
            options_vec.insert(options_vec.begin() + correct_pos, correct_answer);
            
            nlohmann::json opts = nlohmann::json::array();
            for (const auto& opt : options_vec) opts.push_back(opt);
            
            q["options"] = opts;
            q["answer"] = correct_answer;
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

nlohmann::json AiService::buildRoadmap(const std::vector<std::string> & noteIds) const {
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
        return {{"roadmap", nlohmann::json::array()}};
    }

    // Extract key concepts from notes to make roadmap more specific
    std::vector<std::string> all_concepts;
    std::ostringstream notes_summary;
    
    for (const auto & note : filtered_notes) {
        auto concepts = extractConcepts(note);
        all_concepts.insert(all_concepts.end(), concepts.begin(), concepts.end());
        
        // Create a brief summary of the note
        std::string brief = note.content;
        if (brief.size() > 300) brief = brief.substr(0, 297) + "...";
        notes_summary << "- " << note.title << ": " << brief << "\n";
    }

    // Remove duplicates and limit to 10 key concepts
    std::sort(all_concepts.begin(), all_concepts.end());
    all_concepts.erase(std::unique(all_concepts.begin(), all_concepts.end()), all_concepts.end());
    if (all_concepts.size() > 10) all_concepts.resize(10);

    const std::string key_concepts = joinList(all_concepts, ", ");

    // Simplified, focused prompt for better AI compliance
    const std::string prompt =
        "<|system|>\n"
        "You are a learning path designer. Create a practical, deeply detailed roadmap based only on the supplied notes.\n"
        "Output ONLY a valid JSON array. No explanation, no markdown, no code fences.\n"
        "</s>\n"
        "<|user|>\n"
        "NOTES OVERVIEW:\n" + notes_summary.str() + "\n"
        "KEY CONCEPTS: " + key_concepts + "\n\n"
        "Create a learning roadmap with 2-4 modules. Each module must represent a distinct subject area from the notes.\n"
        "For each module:\n"
        "- Module title must be specific and grounded in the note titles or concepts\n"
        "- 2-3 sections with descriptive titles that are not generic\n"
        "- 3-4 concrete, specific topics per section\n"
        "- Every topic must directly reference a concept, example, or idea from the notes\n"
        "- Each topic needs: name, 2-3 sentence description, time estimate, difficulty (Beginner/Medium/Advanced), prerequisites, and a specific learning outcome\n"
        "- Use verbs like compare, derive, apply, implement, troubleshoot, analyze, summarize, or extend when appropriate\n"
        "- Do not repeat the same section title across modules\n"
        "- Do not use vague topic names like Basics, Foundations, Overview, Introduction, or General Concepts\n"
        "- Prefer titles that mention the actual subject from the notes (for example AI, ML, regression, tags, graphs, flashcards, quiz logic)\n\n"
        "Output format:\n"
        "[{\"module_title\":\"...\",\"description\":\"...\",\"total_estimated_time\":\"8 hours\",\"sections\":[{\"section_title\":\"...\",\"estimated_time\":\"3 hours\",\"topics\":[{\"topic_name\":\"...\",\"description\":\"...\",\"estimated_time\":\"45 mins\",\"difficulty\":\"Medium\",\"prerequisites\":[],\"learning_outcome\":\"...\"}]}]}]\n\n"
        "NOW CREATE THE ROADMAP:</s>\n"
        "<|assistant|>\n"
        "[";

    std::string response = ai_engine_.generate(prompt, 2048);
    
    // Attempt JSON parsing
    nlohmann::json roadmap = nlohmann::json::array();
    try {
        auto start = response.find('[');
        auto end = response.rfind(']');
        if (start != std::string::npos && end != std::string::npos && end >= start) {
            std::string json_str = response.substr(start, end - start + 1);
            auto parsed = nlohmann::json::parse(json_str, nullptr, false);
            if (!parsed.is_discarded() && parsed.is_array()) {
                roadmap = parsed;
            }
        }
    } catch (...) {}

    // Smarter Fallback: Generate note-specific roadmaps based on actual content
    if (roadmap.empty()) {
        // Create one module per note so the roadmap stays grounded in the user's content
        for (const auto & note : filtered_notes) {
            nlohmann::json module;
            const auto concepts = extractConcepts(note);
            const std::string primary_topic = !concepts.empty() ? concepts.front() : note.title;
            module["module_title"] = note.title;
            
            std::ostringstream desc;
            desc << "A roadmap built directly from the note content, centered on " << primary_topic << ".";
            module["description"] = desc.str();
            module["total_estimated_time"] = note.content.size() > 800 ? "12 hours" : "8 hours";

            nlohmann::json sections = nlohmann::json::array();

            // Section 1: Core ideas pulled from the note itself
            nlohmann::json sec1;
            sec1["section_title"] = "Core ideas in " + note.title;
            sec1["estimated_time"] = "4 hours";
            nlohmann::json tops1 = nlohmann::json::array();

            const auto sentences = splitSentences(note.content);
            for (std::size_t i = 0; i < concepts.size() && i < 3; ++i) {
                const std::string & concept_name = concepts[i];
                tops1.push_back({
                    {"topic_name", concept_name},
                    {"description", "Explain how " + concept_name + " appears in the note and why it matters."},
                    {"estimated_time", i == 0 ? "45 mins" : "60 mins"},
                    {"difficulty", i == 0 ? "Beginner" : "Medium"},
                    {"prerequisites", nlohmann::json::array()},
                    {"learning_outcome", "Summarize the role of " + concept_name + " in " + note.title}
                });
            }
            if (tops1.empty()) {
                tops1.push_back({
                    {"topic_name", note.title},
                    {"description", summarize(note.content)},
                    {"estimated_time", "60 mins"},
                    {"difficulty", "Beginner"},
                    {"prerequisites", nlohmann::json::array()},
                    {"learning_outcome", "Explain the main idea of " + note.title}
                });
            }
            sec1["topics"] = tops1;
            sections.push_back(sec1);

            // Section 2: Application and practice based on the same note
            nlohmann::json sec2;
            sec2["section_title"] = "Applying " + note.title;
            sec2["estimated_time"] = "3 hours";
            nlohmann::json tops2 = nlohmann::json::array();

            tops2.push_back({
                {"topic_name", "Build a small project from the note"},
                {"description", "Turn the note into a concrete exercise, implementation, or study task."},
                {"estimated_time", "90 mins"},
                {"difficulty", "Medium"},
                {"prerequisites", concepts.empty() ? nlohmann::json::array() : nlohmann::json::array({concepts.front()})},
                {"learning_outcome", "Use the note content in a practical scenario"}
            });

            tops2.push_back({
                {"topic_name", "Common mistakes and clarifications"},
                {"description", "Review confusing parts of the note and identify where misunderstandings usually happen."},
                {"estimated_time", "60 mins"},
                {"difficulty", "Medium"},
                {"prerequisites", nlohmann::json::array()},
                {"learning_outcome", "Spot weak understanding and correct it early"}
            });

            if (!sentences.empty()) {
                tops2.push_back({
                    {"topic_name", "Explain the note in your own words"},
                    {"description", "Practice rewriting the note as a short explanation or summary."},
                    {"estimated_time", "30 mins"},
                    {"difficulty", "Beginner"},
                    {"prerequisites", nlohmann::json::array()},
                    {"learning_outcome", "Restate the note clearly without copying it"}
                });
            }

            sec2["topics"] = tops2;
            sections.push_back(sec2);

            // Section 3: Extension and synthesis
            nlohmann::json sec3;
            sec3["section_title"] = "Extend and connect " + note.title;
            sec3["estimated_time"] = "3 hours";
            nlohmann::json tops3 = nlohmann::json::array();

            tops3.push_back({
                {"topic_name", "Compare with related ideas"},
                {"description", "Compare the note's ideas with nearby concepts from the same field."},
                {"estimated_time", "60 mins"},
                {"difficulty", "Advanced"},
                {"prerequisites", nlohmann::json::array()},
                {"learning_outcome", "Explain how this note differs from related topics"}
            });

            tops3.push_back({
                {"topic_name", "Create follow-up questions"},
                {"description", "Generate questions that would deepen your understanding beyond the original note."},
                {"estimated_time", "45 mins"},
                {"difficulty", "Medium"},
                {"prerequisites", nlohmann::json::array()},
                {"learning_outcome", "Identify gaps and next learning steps"}
            });

            tops3.push_back({
                {"topic_name", "Summarize the full roadmap"},
                {"description", "Turn the whole note into a compact study plan and revision checklist."},
                {"estimated_time", "30 mins"},
                {"difficulty", "Beginner"},
                {"prerequisites", nlohmann::json::array()},
                {"learning_outcome", "Produce a short revision guide from the note"}
            });

            sec3["topics"] = tops3;
            sections.push_back(sec3);

            module["sections"] = sections;
            roadmap.push_back(module);
        }
    }

    return {{"roadmap", roadmap}};
}

nlohmann::json AiService::buildNoteSummary(const std::vector<std::string> & noteIds) const {
    auto all_notes = notes_service_.loadNotes();
    std::vector<NoteRecord> filtered_notes;

    for (const auto & id : noteIds) {
        for (const auto & note : all_notes) {
            if (note.id == id) {
                filtered_notes.push_back(note);
                break;
            }
        }
    }

    if (filtered_notes.empty()) {
        return {{"error", "No notes found"}};
    }

    std::string note_title = "Selected Notes";
    std::ostringstream note_content_stream;
    
    int char_count = 0;
    for (const auto & note : filtered_notes) {
        std::string entry = "NOTE TITLE: " + note.title + "\nCONTENT:\n" + note.content + "\n---\n";
        if (char_count + entry.size() > 4000) {
            note_content_stream << entry.substr(0, 4000 - char_count) << "...";
            break;
        }
        note_content_stream << entry;
        char_count += (int)entry.size();
    }
    std::string note_content = note_content_stream.str();

    const std::string prompt =
        "<|system|>\n"
        "You are an expert summarizer. Read the provided notes and output a structured 'Section-by-Section Summary'.\n"
        "CRITICAL INSTRUCTIONS:\n"
        "1. Summarize the notes BY USING FULL SENTENCES in your own words. DO NOT just list keywords or bullet points.\n"
        "2. Do not write 'meta-text' (e.g. do not say 'This section explains...'). Just provide the factual summary.\n"
        "3. Output EXACTLY valid JSON. No markdown formatting, no code blocks.\n"
        "4. Your output MUST be a JSON object with EXACTLY two keys: 'overall_summary' and 'sections'. DO NOT output any other keys.\n"
        "   - 'overall_summary' (string): A detailed 3-sentence overview of the entire content.\n"
        "   - 'sections' (array of objects): Each object must have 'heading' (string, unique topic name) and 'summary' (string, a factual paragraph summarizing that specific topic).\n"
        "5. DO NOT REPEAT SECTIONS. Merge similar topics together. Ensure the JSON is properly closed with '}' at the end.\n"
        "Input Notes Content:\n" + note_content + "\n"
        "</s>\n"
        "<|user|>\n"
        "Read the notes and generate the structured JSON summary now. ONLY output the raw JSON object starting with { and ending with }.</s>\n"
        "<|assistant|>\n"
        "{";

    std::string response = ai_engine_.generate(prompt, 2048);

    if (response.find("```json") != std::string::npos) {
        auto start_json = response.find("```json") + 7;
        auto end_json = response.find("```", start_json);
        if (end_json != std::string::npos) {
            response = response.substr(start_json, end_json - start_json);
        }
    } else if (response.find("```") != std::string::npos) {
        auto start_json = response.find("```") + 3;
        auto end_json = response.find("```", start_json);
        if (end_json != std::string::npos) {
            response = response.substr(start_json, end_json - start_json);
        }
    }

    // Trim leading whitespace
    response.erase(response.begin(), std::find_if(response.begin(), response.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));

    if (response.empty() || response[0] != '{') {
        response = "{" + response;
    }

    nlohmann::json summary_json;
    try {
        auto start = response.find('{');
        auto end = response.rfind('}');
        if (start != std::string::npos && end != std::string::npos && end >= start) {
            std::string json_str = response.substr(start, end - start + 1);
            auto parsed = nlohmann::json::parse(json_str, nullptr, false);
            if (!parsed.is_discarded() && parsed.is_object()) {
                summary_json = parsed;
            }
        }
    } catch (...) {}

    if (summary_json.empty() || !summary_json.contains("overall_summary")) {
        std::string fallback_text = response;
        if (fallback_text.empty() || fallback_text == "{") {
            // If AI failed completely, fallback to original content without truncation
            fallback_text = note_content;
        } else {
            // Strip the leading `{` if it's there and unclosed
            if (fallback_text[0] == '{') {
                fallback_text = fallback_text.substr(1);
            }
        }
        
        summary_json["overall_summary"] = "The AI generated a summary, but the format could not be parsed correctly. Here is the raw output:\n\n" + fallback_text;
        nlohmann::json sec = nlohmann::json::object();
        sec["heading"] = "Raw Output";
        sec["summary"] = "Please try summarizing again, or select fewer notes if the response was cut off.";
        summary_json["sections"] = {sec};
    }

    return summary_json;
}
#pragma once

#include "../core/ai_engine.h"
#include "notes_service.h"

#include <nlohmann/json.hpp>

#include <string>

class AiService {
public:
    AiService(NotesService & notes_service, AiEngine & ai_engine);

    nlohmann::json buildSearchResponse(const std::string & query, const std::string & mode, const std::string & persona) const;
    nlohmann::json buildInsights() const;
    nlohmann::json buildFlashcards(int count = 5, const std::string & difficulty = "medium", const std::vector<std::string> & noteIds = {}) const;
    nlohmann::json buildGraph() const;
    nlohmann::json buildContradictions() const;
    nlohmann::json buildLearningPath() const;
    nlohmann::json buildQuiz(int count = 5, const std::string & difficulty = "medium", const std::vector<std::string> & noteIds = {}) const;
    nlohmann::json buildSelfQuestions() const;
    nlohmann::json buildIdeas() const;

private:
    NotesService & notes_service_;
    AiEngine & ai_engine_;

    static std::string summarize(const std::string & content);
    static std::vector<std::string> extractConcepts(const NoteRecord & note);
    static std::vector<std::string> splitSentences(const std::string & content);
    static std::string joinList(const std::vector<std::string> & items, const std::string & delimiter);
    static std::string modeInstruction(const std::string & mode, const std::string & persona);
    static std::string fallbackAnswer(const std::string & query, const std::vector<NoteRecord> & notes);
};
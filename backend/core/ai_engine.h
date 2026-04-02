#pragma once

#include <string>

class AiEngine {
public:
    AiEngine(std::string binary_path = {}, std::string model_path = {});

    bool isConfigured() const;
    std::string generate(const std::string & prompt, int max_tokens = 256) const;

private:
    std::string binary_path_;
    std::string model_path_;

    static std::string shellQuote(const std::string & value);
    static std::string writePromptFile(const std::string & prompt);
};
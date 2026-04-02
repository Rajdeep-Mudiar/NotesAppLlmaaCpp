#include "ai_engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#include <cstdio>
#define popen _popen
#define pclose _pclose
#endif

namespace fs = std::filesystem;

AiEngine::AiEngine(std::string binary_path, std::string model_path)
        : binary_path_(binary_path.empty() ? (std::getenv("SECOND_BRAIN_LLAMA_BINARY") ? std::getenv("SECOND_BRAIN_LLAMA_BINARY") : "llama-cli") : std::move(binary_path)),
            model_path_(model_path.empty() ? (std::getenv("SECOND_BRAIN_MODEL_PATH") ? std::getenv("SECOND_BRAIN_MODEL_PATH") : "../../models/model.gguf") : std::move(model_path)) {
}

bool AiEngine::isConfigured() const {
    return !binary_path_.empty() && !model_path_.empty();
}

std::string AiEngine::shellQuote(const std::string & value) {
#if defined(_WIN32)
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') {
            quoted += '\\';
        }
        quoted += ch;
    }
    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
#endif
}

std::string AiEngine::writePromptFile(const std::string & prompt) {
    const auto temp_dir = fs::temp_directory_path();
    const auto file_path = temp_dir / ("second_brain_prompt_" + std::to_string(static_cast<long long>(std::chrono::system_clock::now().time_since_epoch().count())) + ".txt");
    std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
    file << prompt;
    return file_path.string();
}

std::string AiEngine::generate(const std::string & prompt, int max_tokens) const {
    if (!isConfigured()) {
        return {};
    }

    const std::string prompt_file = writePromptFile(prompt);
    const std::string command = shellQuote(binary_path_)
        + " -m " + shellQuote(model_path_)
        + " -f " + shellQuote(prompt_file)
        + " --no-display-prompt"
        + " -n " + std::to_string(max_tokens)
        + " --temp 0.2 --repeat-penalty 1.1";

    std::string output;
    if (FILE * pipe = popen(command.c_str(), "r"); pipe != nullptr) {
        char buffer[4096];
        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        pclose(pipe);
    }

    std::error_code ec;
    fs::remove(prompt_file, ec);

    return output;
}
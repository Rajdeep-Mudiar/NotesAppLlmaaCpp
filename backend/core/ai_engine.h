#pragma once

#include <string>

class AiEngine {
public:
    // binary_path / model_path kept for backward-compat with server.cpp construction.
    // The engine now prefers talking to llama-server over HTTP (fast path).
    // Falls back to spawning llama-cli directly if the server is unreachable (slow path).
    AiEngine(std::string binary_path = {}, std::string model_path = {});

    bool isConfigured() const;
    std::string generate(const std::string & prompt, int max_tokens = 256) const;

private:
    std::string binary_path_;
    std::string model_path_;
    std::string server_host_;  // llama-server host (default 127.0.0.1)
    int         server_port_;  // llama-server port (default 8081)

    // HTTP fast path: POST /completion to a running llama-server instance.
    // Returns the generated text, or empty string on failure.
    std::string generateViaServer(const std::string & prompt, int max_tokens) const;

    // Process slow path: spawn llama-cli, capture output via pipe.
    std::string generateViaCli(const std::string & prompt, int max_tokens) const;

    static std::string shellQuote(const std::string & value);
    static std::string toShortPath(const std::string & value);
    static std::string writePromptFile(const std::string & prompt);
    static std::string cleanLlamaOutput(const std::string & raw);
};
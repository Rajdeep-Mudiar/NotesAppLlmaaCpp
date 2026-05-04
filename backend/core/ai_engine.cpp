#include "ai_engine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

// nlohmann/json is available via the vendor include path
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AiEngine::AiEngine(std::string binary_path, std::string model_path)
    : binary_path_(binary_path.empty()
          ? (std::getenv("SECOND_BRAIN_LLAMA_BINARY")
              ? std::getenv("SECOND_BRAIN_LLAMA_BINARY") : "llama-cli")
          : std::move(binary_path)),
      model_path_(model_path.empty()
          ? (std::getenv("SECOND_BRAIN_MODEL_PATH")
              ? std::getenv("SECOND_BRAIN_MODEL_PATH") : "../models/model.gguf")
          : std::move(model_path)),
      server_host_("127.0.0.1"),
      server_port_(8081)
{
    // Allow overriding the llama-server port via env var
    if (const char * port_env = std::getenv("SECOND_BRAIN_LLAMA_SERVER_PORT")) {
        const int p = std::atoi(port_env);
        if (p > 0 && p < 65536) {
            server_port_ = p;
        }
    }
}

bool AiEngine::isConfigured() const {
    return !binary_path_.empty() && !model_path_.empty();
}

// ---------------------------------------------------------------------------
// Helpers shared by both paths
// ---------------------------------------------------------------------------

std::string AiEngine::shellQuote(const std::string & value) {
#if defined(_WIN32)
    std::string quoted = "\"";
    for (char ch : value) {
        if (ch == '"') { quoted += '\\'; }
        quoted += ch;
    }
    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') { quoted += "'\\''"; }
        else { quoted += ch; }
    }
    quoted += "'";
    return quoted;
#endif
}

std::string AiEngine::toShortPath(const std::string & value) {
#if defined(_WIN32)
    const int wide_length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (wide_length <= 0) { return value; }
    std::wstring wide_value(static_cast<std::size_t>(wide_length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide_value.data(), wide_length) <= 0) { return value; }
    std::wstring short_value(static_cast<std::size_t>(MAX_PATH) * 4, L'\0');
    const DWORD short_length = GetShortPathNameW(wide_value.c_str(), short_value.data(), static_cast<DWORD>(short_value.size()));
    if (short_length == 0 || short_length >= short_value.size()) { return value; }
    short_value.resize(short_length);
    const int utf8_length = WideCharToMultiByte(CP_UTF8, 0, short_value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8_length <= 0) { return value; }
    std::string utf8_value(static_cast<std::size_t>(utf8_length - 1), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, short_value.c_str(), -1, utf8_value.data(), utf8_length, nullptr, nullptr) <= 0) { return value; }
    return utf8_value;
#else
    return value;
#endif
}

std::string AiEngine::writePromptFile(const std::string & prompt) {
    const auto temp_dir  = fs::temp_directory_path();
    const auto file_path = temp_dir / ("second_brain_prompt_"
        + std::to_string(static_cast<long long>(
              std::chrono::system_clock::now().time_since_epoch().count()))
        + ".txt");
    std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
    file << prompt;
    return file_path.string();
}

std::string AiEngine::cleanLlamaOutput(const std::string & raw) {
    if (raw.empty()) return {};

    // 1. Noise patterns (logs, warnings, performance stats)
    static const std::vector<std::string> noise_patterns = {
        "llama_", "ggml_", "gguf_", "load_tensors", "load_backend",
        "main:", "system_info", "sampling", "generate:", "[end of text]",
        "build :", "model :", "modalities :", "available commands:",
        "/exit", "stop or exit", "/regen", "/clear", "/read", "/glob", ">",
        "Press", "Enter", "...", "Loading model", "error:", "warning:",
        "no usable GPU found", "consult docs/build.md", "ignore",
        "exceeds the available context size", "[ Prompt:", "Generation:"
    };

    // 2. High-precision Sentinel Search
    static const std::vector<std::string> sentinels = {"@@@ANSWER_START@@@", "@@@JSON_START@@@"};
    std::size_t last_sentinel_pos = std::string::npos;
    for (const auto & s : sentinels) {
        auto pos = raw.rfind(s);
        if (pos != std::string::npos) {
            if (last_sentinel_pos == std::string::npos || pos > last_sentinel_pos) {
                last_sentinel_pos = pos + s.length();
            }
        }
    }

    std::string filtered;
    if (last_sentinel_pos != std::string::npos) {
        // We found a sentinel! Use everything after it.
        filtered = raw.substr(last_sentinel_pos);
    } else {
        // No sentinel found. Try to find the last occurrence of common instructions as a fallback.
        static const std::vector<std::string> start_markers = {
            "### Response:",
            "Answer (be concise and cite note titles):",
            "JSON Flashcards:",
            "Result:"
        };
        std::size_t last_marker_pos = std::string::npos;
        for (const auto & marker : start_markers) {
            auto pos = raw.rfind(marker);
            if (pos != std::string::npos) {
                if (last_marker_pos == std::string::npos || pos > last_marker_pos) {
                    last_marker_pos = pos + marker.length();
                }
            }
        }
        
        if (last_marker_pos != std::string::npos) {
            filtered = raw.substr(last_marker_pos);
        } else {
            // Last resort: Brutal echo stripping
            filtered = raw;
            static const std::vector<std::string> echo_markers = {
                "### Context (Notes):", "Notes:", "User question:", 
                "### Question:", "Respond like", "You are an AI",
                "[Title]", "[Tags]", "[Content]",
                "<|system|>", "<|user|>", "<|assistant|>", "</s>", "assistant\n"
            };
            
            // We want to find the LAST occurrence of ANY of these and strip everything before it
            std::size_t final_strip_pos = std::string::npos;
            for (const auto & marker : echo_markers) {
                auto pos = filtered.rfind(marker);
                if (pos != std::string::npos) {
                    if (final_strip_pos == std::string::npos || pos > final_strip_pos) {
                        final_strip_pos = pos;
                    }
                }
            }

            if (final_strip_pos != std::string::npos) {
                // Find the end of that line
                auto line_end = filtered.find('\n', final_strip_pos);
                if (line_end != std::string::npos) {
                    filtered = filtered.substr(line_end + 1);
                } else {
                    // Try to find the next meaningful content after the marker
                    filtered = filtered.substr(final_strip_pos);
                    // If it still starts with the marker, strip it manually
                    for (const auto & marker : echo_markers) {
                        if (filtered.find(marker) == 0) {
                            filtered = filtered.substr(marker.length());
                            break;
                        }
                    }
                }
            }
        }
    }

    // 3. Final Noise Filter & Formatting
    std::istringstream stream(filtered);
    std::ostringstream clean;
    std::string line;
    
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        
        // Remove ANSI escape codes
        while (true) {
            auto esc = line.find("\033");
            if (esc == std::string::npos) break;
            auto m = line.find('m', esc);
            if (m == std::string::npos) break;
            line.erase(esc, m - esc + 1);
        }

        // Strip ASCII art blocks
        int block_chars = 0;
        for (unsigned char c : line) if (c >= 0x80) block_chars++;
        if (block_chars > 8) continue; 

        bool is_noise = false;
        for (const auto & pattern : noise_patterns) {
            if (line.find(pattern) != std::string::npos) {
                is_noise = true;
                break;
            }
        }

        if (!is_noise && !line.empty()) {
            clean << line << '\n';
        }
    }

    std::string result = clean.str();
    const auto first = result.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = result.find_last_not_of(" \t\r\n");
    return result.substr(first, last - first + 1);
}

// ---------------------------------------------------------------------------
// FAST PATH: HTTP POST to a running llama-server instance
// ---------------------------------------------------------------------------
// llama-server keeps the model hot in RAM.  After the initial load (~30-60 s)
// every subsequent request completes in ~1-5 seconds instead of ~60-120 s.
//
// API: POST http://127.0.0.1:8081/completion
//      Body: { "prompt": "...", "n_predict": N, "temperature": 0.2,
//              "repeat_penalty": 1.1, "stream": false }
//      Response: { "content": "...", ... }
// ---------------------------------------------------------------------------

std::string AiEngine::generateViaServer(const std::string & prompt, int max_tokens) const {
#if defined(_WIN32)
    std::cerr << "[FAST] Connecting to llama-server at " << server_host_ << ":" << server_port_ << std::endl;
    
    // Build JSON request body
    nlohmann::json req;
    req["prompt"]        = prompt;
    req["n_predict"]     = max_tokens;
    req["temperature"]   = 0.5;
    req["repeat_penalty"]= 1.1;
    req["stream"]        = false;
    req["stop"]          = nlohmann::json::array({"</s>", "<|im_end|>", "<|end|>", "###"});
    const std::string body = req.dump();

    // Open TCP connection to llama-server
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[FAST] socket() failed: " << WSAGetLastError() << std::endl;
        return {};
    }

    // Set a generous receive timeout (model can take a while to generate)
    DWORD timeout_ms = 180000; // 3 minutes
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char *>(&timeout_ms), sizeof(timeout_ms));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(server_port_));
    inet_pton(AF_INET, server_host_.c_str(), &addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        std::cerr << "[FAST] Cannot connect to llama-server (is it running?)" << std::endl;
        closesocket(sock);
        return {};
    }

    // Build HTTP/1.0 request
    std::ostringstream http_req;
    http_req << "POST /completion HTTP/1.0\r\n"
             << "Host: " << server_host_ << ":" << server_port_ << "\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    const std::string request_str = http_req.str();

    if (send(sock, request_str.c_str(), static_cast<int>(request_str.size()), 0) == SOCKET_ERROR) {
        std::cerr << "[FAST] send() failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        return {};
    }

    // Read full response
    std::string raw_response;
    char buffer[8192];
    int n = 0;
    while (true) {
        n = recv(sock, buffer, sizeof(buffer), 0);
        if (n > 0) {
            raw_response.append(buffer, static_cast<std::size_t>(n));
        } else if (n == 0) {
            break; // Closed by server
        } else {
            std::cerr << "[FAST] recv() error: " << WSAGetLastError() << std::endl;
            break;
        }
    }
    closesocket(sock);

    if (raw_response.empty()) {
        std::cerr << "[FAST] Empty response from llama-server" << std::endl;
        return {};
    }

    // Extract HTTP body
    const auto body_start = raw_response.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        std::cerr << "[FAST] Malformed HTTP response" << std::endl;
        return {};
    }
    const std::string json_body = raw_response.substr(body_start + 4);

    try {
        const auto j = nlohmann::json::parse(json_body);
        if (!j.is_object() || !j.contains("content")) {
            std::cerr << "[FAST] Unexpected JSON structure: " << json_body.substr(0, 100) << std::endl;
            return {};
        }
        return j["content"].get<std::string>();
    } catch (const std::exception& e) {
        std::cerr << "[FAST] JSON/Parsing error: " << e.what() << std::endl;
        return {};
    }
#else
    return {};
#endif
}

// ---------------------------------------------------------------------------
// SLOW PATH: spawn llama-cli as a subprocess, capture its stdout+stderr
// (kept as a reliable fallback when llama-server is not running)
// ---------------------------------------------------------------------------

std::string AiEngine::generateViaCli(const std::string & prompt, int max_tokens) const {
    const std::string prompt_file = writePromptFile(prompt);
    const std::string command = shellQuote(binary_path_)
        + " -m " + shellQuote(model_path_)
        + " -f " + shellQuote(prompt_file)
        + " --no-display-prompt"
        + " --log-disable"
        + " -ngl 0"
        + " -n " + std::to_string(max_tokens)
        + " --temp 0.2 --repeat-penalty 1.1";

    std::cerr << "[SLOW] Spawning llama-cli (consider starting llama-server for speed)" << std::endl;
    std::cerr << "[SLOW] CMD: " << command << std::endl;

    std::string output;

#if defined(_WIN32)
    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = nullptr, write_pipe = nullptr;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        std::error_code ec; fs::remove(prompt_file, ec);
        return {};
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nul_handle = CreateFileA("NUL", GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES;
    si.hStdOutput  = write_pipe;
    si.hStdError   = write_pipe;
    si.hStdInput   = (nul_handle != INVALID_HANDLE_VALUE) ? nul_handle : nullptr;

    PROCESS_INFORMATION pi{};
    std::string mutable_cmd = command;
    mutable_cmd.push_back('\0');

    if (CreateProcessA(nullptr, mutable_cmd.data(), nullptr, nullptr,
                       TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) != 0) {
        CloseHandle(write_pipe);
        if (nul_handle != INVALID_HANDLE_VALUE) { CloseHandle(nul_handle); }

        char buf[4096]; DWORD bytes_read = 0;
        while (ReadFile(read_pipe, buf, sizeof(buf), &bytes_read, nullptr) && bytes_read > 0) {
            output.append(buf, buf + bytes_read);
        }

        const DWORD wait = WaitForSingleObject(pi.hProcess, 120000);
        if (wait == WAIT_TIMEOUT) {
            std::cerr << "[SLOW] llama-cli timed out; terminating" << std::endl;
            TerminateProcess(pi.hProcess, 1);
        } else {
            DWORD exit_code = 0;
            GetExitCodeProcess(pi.hProcess, &exit_code);
            std::cerr << "[SLOW] llama-cli exited with code: " << exit_code << std::endl;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        std::cerr << "[SLOW] CreateProcessA failed: " << GetLastError() << std::endl;
        CloseHandle(write_pipe);
        if (nul_handle != INVALID_HANDLE_VALUE) { CloseHandle(nul_handle); }
    }
    CloseHandle(read_pipe);
#else
    if (FILE * pipe = popen(command.c_str(), "r"); pipe != nullptr) {
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), pipe) != nullptr) { output += buf; }
        pclose(pipe);
    }
#endif

    std::error_code ec; fs::remove(prompt_file, ec);
    return cleanLlamaOutput(output);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::string AiEngine::generate(const std::string & prompt, int max_tokens) const {
    if (!isConfigured()) { return {}; }

    // Try the fast HTTP path first (llama-server must be running on port 8081)
    const std::string fast = generateViaServer(prompt, max_tokens);
    if (!fast.empty()) {
        return fast;
    }

    // Fall back to the slow subprocess path
    return generateViaCli(prompt, max_tokens);
}
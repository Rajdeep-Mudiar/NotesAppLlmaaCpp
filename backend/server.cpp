#include "core/ai_engine.h"
#include "services/ai_service.h"
#include "services/notes_service.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = UINT_PTR;
static constexpr socket_t invalid_socket = INVALID_SOCKET;
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t invalid_socket = -1;
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
void loadEnv(const std::string & filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            // Trim whitespace/quotes
            auto trim = [](std::string s) {
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                    return !std::isspace(ch) && ch != '"' && ch != '\'';
                }));
                s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                    return !std::isspace(ch) && ch != '"' && ch != '\'';
                }).base(), s.end());
                return s;
            };
            key = trim(key);
            value = trim(value);
#if defined(_WIN32)
            _putenv_s(key.c_str(), value.c_str());
#else
            setenv(key.c_str(), value.c_str(), 1);
#endif
        }
    }
}

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct AppContext {
    NotesService notes_service;
    AiEngine ai_engine;
    AiService ai_service;

    AppContext(const std::filesystem::path & executable_dir)
        : notes_service(resolveDataDir(executable_dir), executable_dir.string()), ai_engine(resolveLlamaBinary(executable_dir), resolveModelPath(executable_dir)), ai_service(notes_service, ai_engine) {}

    static std::string resolveDataDir(const std::filesystem::path & executable_dir) {
        if (const char * env = std::getenv("SECOND_BRAIN_DATA_DIR")) {
            return env;
        }
        return (executable_dir / ".." / "data" / "notes").lexically_normal().string();
    }

    static std::string resolveLlamaBinary(const std::filesystem::path & executable_dir) {
        const char * env = std::getenv("SECOND_BRAIN_LLAMA_BINARY");
        if (env != nullptr) {
            const fs::path env_path = env;
            if (fs::exists(env_path)) {
                return env_path.lexically_normal().string();
            }
        }

        const std::vector<fs::path> candidates = {
            executable_dir / ".." / "llama.cpp" / "build" / "bin" / "Debug" / "llama-cli.exe",
            executable_dir / ".." / "llama.cpp" / "build" / "bin" / "llama-cli.exe",
            executable_dir / ".." / "llama.cpp" / "build" / "bin" / "Release" / "llama-cli.exe",
            executable_dir / ".." / "llama.cpp" / "build" / "bin" / "llama-cli",
        };

        for (const auto & candidate : candidates) {
            if (fs::exists(candidate)) {
                return candidate.lexically_normal().string();
            }
        }

        return env ? env : "llama-cli";
    }

    static std::string resolveModelPath(const std::filesystem::path & executable_dir) {
        const char * env = std::getenv("SECOND_BRAIN_MODEL_PATH");
        if (env != nullptr) {
            const fs::path env_path = env;
            if (fs::exists(env_path)) {
                return env_path.lexically_normal().string();
            }
        }

        const std::vector<fs::path> candidates = {
            executable_dir / ".." / "models" / "model.gguf",
            executable_dir / "models" / "model.gguf",
        };

        for (const auto & candidate : candidates) {
            if (fs::exists(candidate)) {
                return candidate.lexically_normal().string();
            }
        }

        return env ? env : "models/model.gguf";
    }
};

std::string trim(const std::string & s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::vector<std::string> splitLines(const std::string & data) {
    std::vector<std::string> lines;
    std::stringstream stream(data);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

HttpRequest parseRequest(const std::string & raw) {
    HttpRequest request;
    const auto separator = raw.find("\r\n\r\n");
    const std::string header_block = separator == std::string::npos ? raw : raw.substr(0, separator);
    request.body = separator == std::string::npos ? std::string{} : raw.substr(separator + 4);

    const auto lines = splitLines(header_block);
    if (lines.empty()) {
        return request;
    }

    std::stringstream first_line(lines.front());
    first_line >> request.method >> request.path;
    const auto query_pos = request.path.find('?');
    if (query_pos != std::string::npos) {
        request.path = request.path.substr(0, query_pos);
    }

    for (std::size_t i = 1; i < lines.size(); ++i) {
        const auto colon = lines[i].find(':');
        if (colon == std::string::npos) {
            continue;
        }
        auto key = lower(trim(lines[i].substr(0, colon)));
        auto value = trim(lines[i].substr(colon + 1));
        request.headers[key] = value;
    }
    return request;
}

std::string statusText(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

std::string makeResponse(int status, const json & body) {
    std::string payload;
    try {
        // Use error_handler_t::replace to handle invalid UTF-8 from AI response
        payload = body.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    } catch (const std::exception& e) {
        std::cerr << "[CRITICAL] JSON dump failed: " << e.what() << ". Falling back to safe error message." << std::endl;
        payload = "{\"error\": \"Internal server error during JSON serialization\"}";
        status = 500;
    }
    
    std::ostringstream response;
    response << "HTTP/1.1 " << status << " " << statusText(status) << "\r\n";
    response << "Content-Type: application/json; charset=utf-8\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n";
    response << "Content-Length: " << payload.size() << "\r\n";
    response << "Connection: close\r\n\r\n";
    response << payload;
    return response.str();
}

std::string makeSseHeaders() {
    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/event-stream\r\n";
    response << "Cache-Control: no-cache\r\n";
    response << "Connection: close\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
    return response.str();
}

std::string makeSseData(const json & payload) {
    return "data: " + payload.dump() + "\n\n";
}

std::string readFromSocket(socket_t client_socket) {
    std::string data;
    char buffer[8192];
    while (true) {
        const int received = static_cast<int>(recv(client_socket, buffer, sizeof(buffer), 0));
        if (received <= 0) {
            break;
        }
        data.append(buffer, received);
        
        if (data.find("\r\n\r\n") != std::string::npos) {
            if (data.find("GET") == 0 || data.find("OPTIONS") == 0) break;
            
            auto cl_pos = data.find("Content-Length:");
            if (cl_pos == std::string::npos) {
                // For POST without Content-Length, we might have issues, but let's assume end for now
                break;
            } else {
                auto cl_end = data.find("\r\n", cl_pos);
                if (cl_end != std::string::npos) {
                    try {
                        std::string len_str = trim(data.substr(cl_pos + 15, cl_end - (cl_pos + 15)));
                        if (!len_str.empty()) {
                            int expected_length = std::stoi(len_str);
                            auto body_start = data.find("\r\n\r\n") + 4;
                            if (data.size() - body_start >= static_cast<std::size_t>(expected_length)) break;
                        }
                    } catch (...) {
                        break; // Malformed Content-Length, stop reading
                    }
                }
            }
        }
    }
    return data;
}

bool sendAll(socket_t socket, const std::string & data) {
    std::size_t total_sent = 0;
    while (total_sent < data.size()) {
        const int sent = static_cast<int>(send(socket, data.data() + total_sent, static_cast<int>(data.size() - total_sent), 0));
        if (sent <= 0) {
            return false;
        }
        total_sent += sent;
    }
    return true;
}

json parseJsonBody(const std::string & body) {
    try {
        return json::parse(body);
    } catch (...) {
        return json::object();
    }
}

json handleRequest(const HttpRequest & request, AppContext & app) {
    try {
        if (request.method == "OPTIONS") {
            return json::object();
        }

        if (request.path == "/health") {
            json res = json::object();
            res["status"] = "ok";
            res["notes_dir"] = app.notes_service.baseDirectory();
            return res;
        }

        if (request.method == "GET" && request.path == "/notes") {
            json notes = json::array();
            for (const auto & note : app.notes_service.loadNotes()) {
                notes.push_back(note);
            }
            json res = json::object();
            res["notes"] = notes;
            return res;
        }

        if (request.method == "POST" && (request.path == "/add" || request.path == "/save-note")) {
            std::cout << "[DEBUG] Raw Body: " << request.body << std::endl;
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            const auto title = body.value("title", "");
            const auto content = body.value("content", "");
            
            std::vector<std::string> tags;
            if (body.contains("tags") && body["tags"].is_array()) {
                tags = body["tags"].get<std::vector<std::string>>();
            }

            if (id.empty()) {
                const auto note = app.notes_service.addNote(title, content, tags);
                json res = json::object();
                res["note"] = note;
                return res;
            } else {
                const bool updated = app.notes_service.updateNote(id, title, content, tags);
                json res = json::object();
                res["updated"] = updated;
                return res;
            }
        }

        if (request.method == "POST" && (request.path == "/delete" || request.path == "/delete-note")) {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            const bool deleted = app.notes_service.deleteNote(id);
            json res = json::object();
            res["deleted"] = deleted;
            return res;
        }

        if (request.method == "POST" && request.path == "/insights") {
            return app.ai_service.buildInsights();
        }

        if (request.method == "POST" && request.path == "/search") {
            const auto body = parseJsonBody(request.body);
            const auto query = body.value("query", "");
            const auto mode = body.value("mode", "chat");
            const auto persona = body.value("persona", "teacher");
            return app.ai_service.buildSearchResponse(query, mode, persona);
        }

        if (request.method == "POST" && request.path == "/flashcards") {
            const auto body = parseJsonBody(request.body);
            const int count = body.value("count", 5);
            const std::string difficulty = body.value("difficulty", "medium");
            std::vector<std::string> noteIds;
            if (body.contains("noteIds") && body["noteIds"].is_array()) {
                noteIds = body["noteIds"].get<std::vector<std::string>>();
            }
            return app.ai_service.buildFlashcards(count, difficulty, noteIds);
        }

        if (request.method == "POST" && request.path == "/quiz") {
            const auto body = parseJsonBody(request.body);
            const int count = body.value("count", 5);
            const std::string difficulty = body.value("difficulty", "medium");
            std::vector<std::string> noteIds;
            if (body.contains("noteIds") && body["noteIds"].is_array()) {
                noteIds = body["noteIds"].get<std::vector<std::string>>();
            }
            return app.ai_service.buildQuiz(count, difficulty, noteIds);
        }

        if (request.method == "POST" && request.path == "/graph") {
            return app.ai_service.buildGraph();
        }

        if (request.method == "POST" && request.path == "/contradictions") {
            return app.ai_service.buildContradictions();
        }

        if (request.method == "POST" && request.path == "/learning-path") {
            return app.ai_service.buildLearningPath();
        }

        if (request.method == "POST" && request.path == "/history") {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            json res = json::object();
            res["versions"] = app.notes_service.noteHistory(id);
            return res;
        }

        if (request.method == "POST" && request.path == "/restore-version") {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            const auto version_file = body.value("version_file", "");
            const bool restored = app.notes_service.restoreVersion(id, version_file);
            json res = json::object();
            res["restored"] = restored;
            return res;
        }

        if (request.method == "POST" && request.path == "/questions") {
            return app.ai_service.buildSelfQuestions();
        }

        if (request.method == "POST" && request.path == "/ideas") {
            return app.ai_service.buildIdeas();
        }

        json res = json::object();
        res["error"] = "Route not found: " + request.path;
        return res;
    } catch (const std::exception & error) {
        std::cerr << "[ERROR] handleRequest [" << request.method << " " << request.path << "]: " << error.what() << std::endl;
        json res = json::object();
        res["error"] = error.what();
        return res;
    }
}

void closeSocket(socket_t socket_value) {
#if defined(_WIN32)
    closesocket(socket_value);
#else
    close(socket_value);
#endif
}

void handleClient(socket_t client_socket, AppContext & app) {
    try {
        const std::string raw = readFromSocket(client_socket);
        if (raw.empty()) {
            return;
        }

        const auto request = parseRequest(raw);
        std::cout << "[INFO] Request: " << request.method << " " << request.path << std::endl;

        if (request.method == "POST" && request.path == "/search/stream") {
            const auto body = parseJsonBody(request.body);
            const auto query = body.value("query", "");
            const auto mode = body.value("mode", "chat");
            const auto persona = body.value("persona", "teacher");

            const json full = app.ai_service.buildSearchResponse(query, mode, persona);
            const std::string answer = full.value("answer", "");

            if (!sendAll(client_socket, makeSseHeaders())) {
                return;
            }

            json start_msg = json::object();
            start_msg["type"] = "start";
            start_msg["query"] = query;
            sendAll(client_socket, makeSseData(start_msg));

            const std::size_t chunk_size = 16;
            for (std::size_t i = 0; i < answer.size(); i += chunk_size) {
                const std::string chunk = answer.substr(i, chunk_size);
                json token_msg = json::object();
                token_msg["type"] = "token";
                token_msg["content"] = chunk;
                if (!sendAll(client_socket, makeSseData(token_msg))) {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            json end_msg = json::object();
            end_msg["type"] = "end";
            sendAll(client_socket, makeSseData(end_msg));
            return;
        }

        const json response_body = handleRequest(request, app);
        std::cout << "[DEBUG] Finished handleRequest for " << request.path << std::endl;
        const std::string response_data = makeResponse(response_body.contains("error") ? 400 : 200, response_body);
        sendAll(client_socket, response_data);
        std::cout << "[INFO] Sent response for " << request.path << std::endl;
    } catch (const std::exception & e) {
        std::cerr << "[ERROR] handleClient exception: " << e.what() << std::endl;
        try {
            json err_msg = json::object();
            err_msg["error"] = e.what();
            sendAll(client_socket, makeResponse(500, err_msg));
        } catch (...) {}
    } catch (...) {
        std::cerr << "[ERROR] handleClient: Unknown critical error" << std::endl;
        try {
            json err_msg = json::object();
            err_msg["error"] = "Unknown internal error";
            sendAll(client_socket, makeResponse(500, err_msg));
        } catch (...) {}
    }
}

socket_t createServerSocket(int port) {
    socket_t server_socket = static_cast<socket_t>(::socket(AF_INET, SOCK_STREAM, 0));
    if (server_socket == invalid_socket) {
        throw std::runtime_error("Unable to create socket");
    }

    int reuse = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<unsigned short>(port));

    if (bind(server_socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        throw std::runtime_error("Unable to bind socket");
    }

    if (listen(server_socket, 64) < 0) {
        throw std::runtime_error("Unable to listen on socket");
    }

    return server_socket;
}
} // namespace

int main(int argc, char ** argv) {
#if defined(_WIN32)
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Failed to initialize WinSock\n";
        return 1;
    }
#endif

    const fs::path executable_path = argc > 0 ? fs::absolute(argv[0]) : fs::current_path();
    const fs::path executable_dir = executable_path.has_parent_path() ? executable_path.parent_path() : fs::current_path();
    
    // Load .env if present in executable dir or parent
    loadEnv((executable_dir / ".env").string());
    loadEnv((executable_dir / ".." / ".env").string());

    AppContext app(executable_dir);
    const int port = std::getenv("SECOND_BRAIN_PORT") ? std::atoi(std::getenv("SECOND_BRAIN_PORT")) : 8080;

    try {
        const auto server_socket = createServerSocket(port);
        std::cout << "AI Second Brain backend running on http://127.0.0.1:" << port << '\n';

        while (true) {
            sockaddr_in client_address{};
            socklen_t client_length = sizeof(client_address);
            const auto client_socket = static_cast<socket_t>(accept(server_socket, reinterpret_cast<sockaddr *>(&client_address), &client_length));
            if (client_socket == invalid_socket) {
                continue;
            }

            std::thread([client_socket, &app]() {
                try {
                    handleClient(client_socket, app);
                } catch (const std::exception & e) {
                    std::cerr << "[CRITICAL] Worker Thread Error: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[CRITICAL] Worker Thread Unknown Error" << std::endl;
                }
                closeSocket(client_socket);
            }).detach();
        }
    } catch (const std::exception & error) {
        std::cerr << error.what() << '\n';
    }

#if defined(_WIN32)
    WSACleanup();
#endif
    return 0;
}
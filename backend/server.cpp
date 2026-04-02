#include "core/ai_engine.h"
#include "services/ai_service.h"
#include "services/notes_service.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
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
#pragma comment(lib, "Ws2_32.lib")
using socket_t = SOCKET;
static constexpr socket_t invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
static constexpr socket_t invalid_socket = -1;
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
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
        : notes_service(resolveDataDir(executable_dir)), ai_engine("", resolveModelPath(executable_dir)), ai_service(notes_service, ai_engine) {}

    static std::string resolveDataDir(const std::filesystem::path & executable_dir) {
        if (const char * env = std::getenv("SECOND_BRAIN_DATA_DIR")) {
            return env;
        }
        return (executable_dir / ".." / "data" / "notes").lexically_normal().string();
    }

    static std::string resolveModelPath(const std::filesystem::path & executable_dir) {
        if (const char * env = std::getenv("SECOND_BRAIN_MODEL_PATH")) {
            return env;
        }
        return (executable_dir / ".." / ".." / "models" / "model.gguf").lexically_normal().string();
    }
};

std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !is_space(static_cast<unsigned char>(ch)); }).base(), value.end());
    return value;
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
    const std::string payload = body.dump(2);
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
    char buffer[4096];
    while (true) {
        const int received = static_cast<int>(recv(client_socket, buffer, sizeof(buffer), 0));
        if (received <= 0) {
            break;
        }
        data.append(buffer, buffer + received);
        const auto separator = data.find("\r\n\r\n");
        if (separator == std::string::npos) {
            continue;
        }
        const auto header_part = data.substr(0, separator);
        const auto body_length_pos = lower(header_part).find("content-length:");
        if (body_length_pos == std::string::npos) {
            break;
        }
        const auto end_of_line = header_part.find("\r\n", body_length_pos);
        const auto line = header_part.substr(body_length_pos, end_of_line - body_length_pos);
        const auto colon = line.find(':');
        const auto length_text = trim(line.substr(colon + 1));
        const auto length = static_cast<std::size_t>(std::stoull(length_text));
        const auto body_received = data.size() - (separator + 4);
        if (body_received >= length) {
            break;
        }
    }
    return data;
}

bool sendAll(socket_t client_socket, const std::string & response) {
    std::size_t sent_total = 0;
    while (sent_total < response.size()) {
        const int sent = static_cast<int>(send(client_socket, response.data() + sent_total, static_cast<int>(response.size() - sent_total), 0));
        if (sent <= 0) {
            return false;
        }
        sent_total += static_cast<std::size_t>(sent);
    }
    return true;
}

json parseJsonBody(const std::string & body) {
    if (body.empty()) {
        return json::object();
    }
    return json::parse(body);
}

json handleRequest(const HttpRequest & request, AppContext & app) {
    if (request.method == "OPTIONS") {
        return json{{"status", "ok"}};
    }

    try {
        if (request.method == "GET" && request.path == "/health") {
            return json{{"status", "ok"}, {"notes_dir", app.notes_service.baseDirectory()}};
        }

        if (request.method == "GET" && request.path == "/notes") {
            json notes = json::array();
            for (const auto & note : app.notes_service.loadNotes()) {
                notes.push_back(note);
            }
            return json{{"notes", notes}};
        }

        if (request.method == "POST" && request.path == "/add") {
            const auto body = parseJsonBody(request.body);
            const auto title = body.value("title", "");
            const auto content = body.value("content", "");
            std::vector<std::string> tags = body.contains("tags") ? body["tags"].get<std::vector<std::string>>() : std::vector<std::string>{};
            const auto note = app.notes_service.addNote(title, content, tags);
            return json{{"note", note}};
        }

        if (request.method == "POST" && request.path == "/update") {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            const auto title = body.value("title", "");
            const auto content = body.value("content", "");
            std::vector<std::string> tags = body.contains("tags") ? body["tags"].get<std::vector<std::string>>() : std::vector<std::string>{};
            const bool updated = app.notes_service.updateNote(id, title, content, tags);
            return json{{"updated", updated}};
        }

        if (request.method == "POST" && request.path == "/delete") {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            const bool deleted = app.notes_service.deleteNote(id);
            return json{{"deleted", deleted}};
        }

        if (request.method == "POST" && request.path == "/search") {
            const auto body = parseJsonBody(request.body);
            const auto query = body.value("query", "");
            const auto mode = body.value("mode", "chat");
            const auto persona = body.value("persona", "teacher");
            return app.ai_service.buildSearchResponse(query, mode, persona);
        }

        if (request.method == "POST" && request.path == "/history") {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            return json{{"versions", app.notes_service.noteHistory(id)}};
        }

        if (request.method == "POST" && request.path == "/restore-version") {
            const auto body = parseJsonBody(request.body);
            const auto id = body.value("id", "");
            const auto version_file = body.value("version_file", "");
            const bool restored = app.notes_service.restoreVersion(id, version_file);
            return json{{"restored", restored}};
        }

        if (request.method == "POST" && request.path == "/insights") {
            return app.ai_service.buildInsights();
        }

        if (request.method == "POST" && request.path == "/flashcards") {
            return app.ai_service.buildFlashcards();
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

        if (request.method == "POST" && request.path == "/questions") {
            return app.ai_service.buildSelfQuestions();
        }

        if (request.method == "POST" && request.path == "/ideas") {
            return app.ai_service.buildIdeas();
        }

        return json{{"error", "Route not found"}};
    } catch (const std::exception & error) {
        return json{{"error", error.what()}};
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
    const std::string raw = readFromSocket(client_socket);
    if (raw.empty()) {
        return;
    }

    const auto request = parseRequest(raw);

    if (request.method == "POST" && request.path == "/search/stream") {
        try {
            const auto body = parseJsonBody(request.body);
            const auto query = body.value("query", "");
            const auto mode = body.value("mode", "chat");
            const auto persona = body.value("persona", "teacher");

            const json full = app.ai_service.buildSearchResponse(query, mode, persona);
            const std::string answer = full.value("answer", "");

            if (!sendAll(client_socket, makeSseHeaders())) {
                return;
            }

            sendAll(client_socket, makeSseData(json{{"type", "start"}, {"query", query}}));

            const std::size_t chunk_size = 16;
            for (std::size_t i = 0; i < answer.size(); i += chunk_size) {
                const std::string chunk = answer.substr(i, chunk_size);
                if (!sendAll(client_socket, makeSseData(json{{"type", "token"}, {"content", chunk}}))) {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(12));
            }

            sendAll(client_socket, makeSseData(json{{"type", "done"}, {"response", full}}));
            return;
        } catch (const std::exception & error) {
            sendAll(client_socket, makeSseHeaders());
            sendAll(client_socket, makeSseData(json{{"type", "error"}, {"message", error.what()}}));
            return;
        }
    }

    const json body = handleRequest(request, app);
    const int status = body.contains("error") && request.path != "/health" ? 400 : 200;
    const std::string response = makeResponse(status, body);
    sendAll(client_socket, response);
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
                handleClient(client_socket, app);
                closeSocket(client_socket);
            }).detach();
        }
    } catch (const std::exception & error) {
        std::cerr << error.what() << '\n';
    }

#if defined(_WIN32)
    WSACleanup();
#endif
    return 1;
}
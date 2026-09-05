#include "offgrid/ollama.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

#ifndef _WIN32
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace offgrid {
namespace {

#if WAYKEEPER_ENABLE_OLLAMA
#ifndef _WIN32
std::optional<std::string> request(
    const std::string& method, const std::string& path, const std::string& body) {
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) return std::nullopt;

    timeval timeout{120, 0};
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(11434);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(socket_fd);
        return std::nullopt;
    }

    std::string message = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
    if (!body.empty()) {
        message += "Content-Type: application/json\r\nContent-Length: " +
            std::to_string(body.size()) + "\r\n";
    }
    message += "\r\n" + body;
    std::size_t sent = 0;
    while (sent < message.size()) {
        const auto count = send(socket_fd, message.data() + sent, message.size() - sent, 0);
        if (count <= 0) { close(socket_fd); return std::nullopt; }
        sent += static_cast<std::size_t>(count);
    }

    std::string response;
    char buffer[8192];
    for (;;) {
        const auto count = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        response.append(buffer, static_cast<std::size_t>(count));
    }
    close(socket_fd);
    const auto separator = response.find("\r\n\r\n");
    if (separator == std::string::npos || response.find(" 200 ") == std::string::npos) {
        return std::nullopt;
    }
    return response.substr(separator + 4);
}
#endif

std::string json_escape(std::string_view input) {
    std::string output;
    output.reserve(input.size() + 32);
    for (const unsigned char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (c >= 0x20) output.push_back(static_cast<char>(c));
        }
    }
    return output;
}

std::optional<std::string> json_string(const std::string& json, const std::string& key) {
    const std::string marker = "\"" + key + "\":";
    std::size_t cursor = json.find(marker);
    if (cursor == std::string::npos) return std::nullopt;
    cursor = json.find('"', cursor + marker.size());
    if (cursor == std::string::npos) return std::nullopt;
    ++cursor;
    std::string result;
    bool escaped = false;
    while (cursor < json.size()) {
        const char c = json[cursor++];
        if (escaped) {
            switch (c) {
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                default: result.push_back(c); break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return result;
        } else {
            result.push_back(c);
        }
    }
    return std::nullopt;
}
#endif

}  // namespace

OllamaClient::OllamaClient(std::string model) : model_(std::move(model)) {
    if (const char* configured = std::getenv("OFFGRID_OLLAMA_MODEL")) model_ = configured;
}

bool OllamaClient::enabled() const {
    return WAYKEEPER_ENABLE_OLLAMA != 0;
}

bool OllamaClient::server_ready() const {
#if !WAYKEEPER_ENABLE_OLLAMA
    return false;
#elif defined(_WIN32)
    return false;
#else
    return request("GET", "/api/tags", "").has_value();
#endif
}

std::optional<std::string> OllamaClient::generate(
    const std::string_view prompt, std::string& error) const {
#if !WAYKEEPER_ENABLE_OLLAMA
    (void)prompt;
    error = "Ollama is retained in source but compile-disabled for WayKeeper embedded builds.";
    return std::nullopt;
#elif defined(_WIN32)
    error = "The Phase-A Ollama socket adapter currently targets macOS/Linux.";
    return std::nullopt;
#else
    const std::string body = "{\"model\":\"" + json_escape(model_) +
        "\",\"prompt\":\"" + json_escape(prompt) +
        "\",\"stream\":false,\"think\":false}";
    const auto response = request("POST", "/api/generate", body);
    if (!response) {
        error = "Ollama did not answer. Start it with 'ollama serve' and install the configured model.";
        return std::nullopt;
    }
    const auto answer = json_string(*response, "response");
    if (!answer) {
        error = "Ollama returned a response the Phase-A client could not parse. Model: " + model_;
        return std::nullopt;
    }
    return answer;
#endif
}

const std::string& OllamaClient::model() const { return model_; }

}  // namespace offgrid

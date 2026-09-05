#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace offgrid {

class OllamaClient {
public:
    explicit OllamaClient(std::string model = "qwen3:4b");

    bool enabled() const;
    bool server_ready() const;
    std::optional<std::string> generate(std::string_view prompt, std::string& error) const;
    const std::string& model() const;

private:
    std::string model_;
};

}  // namespace offgrid

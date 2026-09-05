#pragma once

#include <cstddef>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace offgrid {

struct TerminalSize {
    std::size_t columns{100};
    std::size_t rows{40};
};

enum class TerminalColorMode {
    Plain,
    Ansi256,
    TrueColor,
};

enum class TerminalInlineImageProtocol {
    None,
    WayTerm,
};

enum class TerminalKeyStatus {
    Key,
    Timeout,
    Unavailable,
    EndOfInput,
};

struct TerminalKeyResult {
    TerminalKeyStatus status{TerminalKeyStatus::Unavailable};
    std::string key;
};

bool enable_ansi_terminal();
TerminalColorMode terminal_color_mode(bool ansi_enabled);
TerminalInlineImageProtocol terminal_inline_image_protocol();
bool emit_terminal_inline_image(
    TerminalInlineImageProtocol protocol, const std::filesystem::path& image,
    std::size_t columns, std::size_t rows, bool ansi_enabled);
TerminalSize terminal_size();
bool request_terminal_pixel_size(int width_pixels, int height_pixels, bool ansi_enabled);
std::optional<std::string> read_pager_key();
TerminalKeyResult read_terminal_key_for(std::chrono::milliseconds timeout);

}  // namespace offgrid

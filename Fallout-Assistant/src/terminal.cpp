#include "offgrid/terminal.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <cstdio>
#include <io.h>
#include <windows.h>
#else
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

namespace offgrid {
namespace {

std::string lower_environment(const char* value) {
    if (!value) return {};
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

bool enabled_environment(const char* value) {
    const auto normalized = lower_environment(value);
    return normalized == "1" || normalized == "on" || normalized == "true" ||
           normalized == "yes" || normalized == "wayterm" || normalized == "inline";
}

std::string percent_encoded_path(const std::filesystem::path& path) {
    static constexpr char hexadecimal[] = "0123456789ABCDEF";
    std::ostringstream encoded;
    for (const unsigned char character : path.string()) {
        if (std::isalnum(character) || character == '/' || character == '-' ||
            character == '_' || character == '.') {
            encoded << static_cast<char>(character);
        } else {
            encoded << '%' << hexadecimal[character >> 4] << hexadecimal[character & 0x0f];
        }
    }
    return encoded.str();
}

}  // namespace

bool enable_ansi_terminal() {
#ifdef _WIN32
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    if (!GetConsoleMode(output, &mode)) return false;
    return SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

TerminalColorMode terminal_color_mode(const bool ansi_enabled) {
    if (!ansi_enabled) return TerminalColorMode::Plain;
    if (const char* configured = std::getenv("OFFGRID_COLOR_MODE")) {
        std::string mode(configured);
        std::transform(mode.begin(), mode.end(), mode.begin(), [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        if (mode == "256" || mode == "8bit" || mode == "ansi256") {
            return TerminalColorMode::Ansi256;
        }
    }
    return TerminalColorMode::TrueColor;
}

TerminalInlineImageProtocol terminal_inline_image_protocol() {
    const auto override = lower_environment(std::getenv("OFFGRID_INLINE_COMPANION"));
    if (override == "0" || override == "off" || override == "false" ||
        override == "ansi" || override == "none") {
        return TerminalInlineImageProtocol::None;
    }
    if (override == "1" || override == "on" || override == "true" ||
        override == "inline" || override == "wayterm") {
        return TerminalInlineImageProtocol::WayTerm;
    }

    if (enabled_environment(std::getenv("WAYTERM_INLINE_IMAGES"))) {
        return TerminalInlineImageProtocol::WayTerm;
    }
    const auto terminal_program = lower_environment(std::getenv("TERM_PROGRAM"));
    const auto terminal = lower_environment(std::getenv("TERM"));
    if (terminal_program == "wayterm" || terminal_program == "waykeeper-wayterm" ||
        terminal.rfind("wayterm", 0) == 0) {
        return TerminalInlineImageProtocol::WayTerm;
    }
    return TerminalInlineImageProtocol::None;
}

bool emit_terminal_inline_image(
    const TerminalInlineImageProtocol protocol, const std::filesystem::path& image,
    const std::size_t columns, const std::size_t rows, const bool ansi_enabled) {
    if (!ansi_enabled || protocol == TerminalInlineImageProtocol::None ||
        image.empty() || !std::filesystem::is_regular_file(image) ||
        columns == 0 || rows == 0) {
        return false;
    }
    switch (protocol) {
        case TerminalInlineImageProtocol::WayTerm:
            // Private WayTerm OSC: render the PNG at the current cursor without moving it.
            // The host owns image decoding and keeps the placement behind subsequent cell text.
            std::cout << "\033]777;waykeeper-inline-image;path="
                      << percent_encoded_path(std::filesystem::absolute(image))
                      << ";columns=" << columns << ";rows=" << rows << "\007";
            return true;
        case TerminalInlineImageProtocol::None:
            return false;
    }
    return false;
}

TerminalSize terminal_size() {
    TerminalSize size;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(output, &info)) {
        size.columns = static_cast<std::size_t>(info.srWindow.Right - info.srWindow.Left + 1);
        size.rows = static_cast<std::size_t>(info.srWindow.Bottom - info.srWindow.Top + 1);
    }
#else
    winsize window{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window) == 0) {
        if (window.ws_col > 0) size.columns = window.ws_col;
        if (window.ws_row > 0) size.rows = window.ws_row;
    }
#endif
    return size;
}

bool request_terminal_pixel_size(
    const int width_pixels, const int height_pixels, const bool ansi_enabled) {
    if (!ansi_enabled || width_pixels <= 0 || height_pixels <= 0) return false;
    std::cout << "\033[4;" << height_pixels << ';' << width_pixels << 't' << std::flush;
    return true;
}

TerminalKeyResult read_terminal_key_for(const std::chrono::milliseconds timeout) {
#ifdef _WIN32
    if (!_isatty(_fileno(stdin))) return {TerminalKeyStatus::Unavailable, {}};
    const auto started = std::chrono::steady_clock::now();
    while (!_kbhit()) {
        if (timeout.count() >= 0 && std::chrono::steady_clock::now() - started >= timeout) {
            return {TerminalKeyStatus::Timeout, {}};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const int first = _getch();
    if (first == 0 || first == 224) {
        switch (_getch()) {
            case 59: return {TerminalKeyStatus::Key, "f1"};
            case 60: return {TerminalKeyStatus::Key, "f2"};
            case 61: return {TerminalKeyStatus::Key, "f3"};
            case 62: return {TerminalKeyStatus::Key, "f4"};
            case 63: return {TerminalKeyStatus::Key, "f5"};
            case 64: return {TerminalKeyStatus::Key, "f6"};
            case 65: return {TerminalKeyStatus::Key, "f7"};
            case 66: return {TerminalKeyStatus::Key, "f8"};
            case 67: return {TerminalKeyStatus::Key, "f9"};
            case 68: return {TerminalKeyStatus::Key, "f10"};
            case 133: return {TerminalKeyStatus::Key, "f11"};
            case 134: return {TerminalKeyStatus::Key, "f12"};
            case 71: return {TerminalKeyStatus::Key, "home"};
            case 72: return {TerminalKeyStatus::Key, "up"};
            case 73: return {TerminalKeyStatus::Key, "pgup"};
            case 75: return {TerminalKeyStatus::Key, "left"};
            case 77: return {TerminalKeyStatus::Key, "right"};
            case 79: return {TerminalKeyStatus::Key, "end"};
            case 80: return {TerminalKeyStatus::Key, "down"};
            case 81: return {TerminalKeyStatus::Key, "pgdn"};
            default: return {TerminalKeyStatus::Key, {}};
        }
    }
    if (first == 27) return {TerminalKeyStatus::Key, "escape"};
    if (first == '\r' || first == '\n') return {TerminalKeyStatus::Key, {}};
    return {TerminalKeyStatus::Key, std::string(1, static_cast<char>(first))};
#else
    if (!isatty(STDIN_FILENO)) return {TerminalKeyStatus::Unavailable, {}};
    static std::optional<char> pending_character;
    termios previous{};
    if (tcgetattr(STDIN_FILENO, &previous) != 0) {
        return {TerminalKeyStatus::Unavailable, {}};
    }
    auto raw = previous;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return {TerminalKeyStatus::Unavailable, {}};
    }

    char first = 0;
    ssize_t count = 0;
    if (pending_character) {
        first = *pending_character;
        pending_character.reset();
        count = 1;
    } else {
        pollfd input{STDIN_FILENO, POLLIN, 0};
        const auto bounded_timeout = timeout.count() < 0 ? -1 :
            static_cast<int>(std::min<long long>(timeout.count(), 2147483647LL));
        const int ready = poll(&input, 1, bounded_timeout);
        if (ready == 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &previous);
            return {TerminalKeyStatus::Timeout, {}};
        }
        if (ready < 0) {
            tcsetattr(STDIN_FILENO, TCSANOW, &previous);
            return {TerminalKeyStatus::EndOfInput, {}};
        }
        count = read(STDIN_FILENO, &first, 1);
    }
    if (count != 1) {
        tcsetattr(STDIN_FILENO, TCSANOW, &previous);
        return {TerminalKeyStatus::EndOfInput, {}};
    }

    std::string sequence(1, first);
    if (first == '\x1b') {
        pollfd input{STDIN_FILENO, POLLIN, 0};
        while (sequence.size() < 8 && poll(&input, 1, 35) > 0) {
            char next = 0;
            if (read(STDIN_FILENO, &next, 1) != 1) break;
            sequence.push_back(next);
        }
    } else if (first == 'g' || first == 'G') {
        pollfd input{STDIN_FILENO, POLLIN, 0};
        if (poll(&input, 1, 180) > 0) {
            char next = 0;
            if (read(STDIN_FILENO, &next, 1) == 1 && (next == 'g' || next == 'G')) {
                sequence.push_back(next);
            } else if (next != 0) {
                pending_character = next;
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &previous);
    if (first == '\r' || first == '\n') return {TerminalKeyStatus::Key, {}};
    return {TerminalKeyStatus::Key, sequence};
#endif
}

std::optional<std::string> read_pager_key() {
    const auto result = read_terminal_key_for(std::chrono::milliseconds{-1});
    if (result.status == TerminalKeyStatus::Unavailable) return std::nullopt;
    return result.key;
}

}  // namespace offgrid

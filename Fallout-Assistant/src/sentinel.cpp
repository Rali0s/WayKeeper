#include "offgrid/sentinel.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace offgrid {

std::string_view sentinel_mode_name(const SentinelMode mode) {
    switch (mode) {
        case SentinelMode::Sentinel: return "SENTINEL";
        case SentinelMode::Quiet: return "QUIET";
        case SentinelMode::Blackout: return "BLACKOUT";
    }
    return "SENTINEL";
}

bool valid_sentinel_movie_speed(const double speed) {
    return std::abs(speed - 0.5) < 0.001 ||
           std::abs(speed - 1.0) < 0.001 ||
           std::abs(speed - 2.0) < 0.001;
}

double next_sentinel_movie_speed(const double speed) {
    if (speed < 0.75) return 1.0;
    if (speed < 1.5) return 2.0;
    return 0.5;
}

std::chrono::milliseconds sentinel_card_duration(const double speed) {
    const auto safe_speed = valid_sentinel_movie_speed(speed) ? speed : 1.0;
    // Twelve seconds at normal speed. Even the slowest setting is far below
    // the five-minute dead-man window.
    return std::chrono::milliseconds{
        static_cast<long long>(std::lround(12000.0 / safe_speed))};
}

std::optional<std::string> load_sentinel_last_will(
    const std::filesystem::path& path, std::string& error) {
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::ostringstream content;
    content << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = "Could not read the Last Will message.";
        return std::nullopt;
    }
    auto message = content.str();
    if (message.size() > sentinel_last_will_max_bytes) {
        error = "Last Will message exceeds the 4096-byte public-display limit.";
        return std::nullopt;
    }
    std::erase_if(message, [](const unsigned char character) {
        return character < 0x20 && character != '\n' && character != '\t';
    });
    if (message.find_first_not_of(" \t\n") == std::string::npos) return std::nullopt;
    return message;
}

bool save_sentinel_last_will(
    const std::filesystem::path& path, const std::string_view message,
    std::string& error) {
    error.clear();
    if (message.empty() || message.size() > sentinel_last_will_max_bytes ||
        message.find_first_not_of(" \t\n") == std::string_view::npos) {
        error = "Last Will message must contain 1-4096 bytes.";
        return false;
    }
    if (std::any_of(message.begin(), message.end(), [](const unsigned char character) {
            return character < 0x20 && character != '\n' && character != '\t';
        })) {
        error = "Last Will message contains unsupported terminal control characters.";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create the Last Will state directory: " +
            filesystem_error.message();
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not write the Last Will message: " + path.string();
        return false;
    }
    output.write(message.data(), static_cast<std::streamsize>(message.size()));
    if (!output) {
        error = "Could not finish writing the Last Will message.";
        return false;
    }
    return true;
}

bool SentinelEscapeSequence::push(const std::string_view key) {
    // POSIX terminals may coalesce rapidly typed bytes after Escape into one
    // terminal sequence. Feed those bytes through the same recognizer so both
    // deliberate key-by-key entry and a fast physical chord work.
    if (key.size() > 1 && key.front() == '\x1b') {
        for (const char character : key) {
            const std::string_view token(&character, 1);
            if (push(token)) return true;
        }
        return false;
    }
    const bool escape = key == "escape" || key == "\x1b";
    const bool enter = key.empty() || key == "enter" || key == "\r" || key == "\n";
    const bool w = key == "w" || key == "W";
    const bool k = key == "k" || key == "K";

    if (position_ == 0 && escape) {
        position_ = 1;
    } else if (position_ == 1 && w) {
        position_ = 2;
    } else if (position_ == 2 && k) {
        position_ = 3;
    } else if (position_ == 3 && enter) {
        reset();
        return true;
    } else {
        position_ = escape ? 1 : 0;
    }
    return false;
}

void SentinelEscapeSequence::reset() { position_ = 0; }

}  // namespace offgrid

#include "offgrid/settings.hpp"

#include "offgrid/profile.hpp"
#include "offgrid/sentinel.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace offgrid {

std::filesystem::path settings_path() {
    return profile_path().parent_path() / "settings.ini";
}

bool valid_terminal_resolution(const int width_pixels, const int height_pixels) {
    return width_pixels >= 640 && width_pixels <= 3840 &&
           height_pixels >= 480 && height_pixels <= 2160;
}

bool valid_terminal_theme(const std::string_view theme) {
    return theme == "blue" || theme == "gold" || theme == "green";
}

bool valid_terminal_layout(const std::string_view layout) {
    return layout == "workstation" || layout == "minimal" || layout == "static";
}

bool valid_companion_render(const std::string_view render) {
    return render == "auto" || render == "ansi" || render == "off";
}

bool valid_room_code(const std::string_view room_code) {
    if (room_code.empty() || room_code.size() > 16) return false;
    return std::all_of(room_code.begin(), room_code.end(), [](const unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_';
    });
}

bool load_terminal_settings(TerminalSettings& settings, std::string& error) {
    std::ifstream input(settings_path());
    if (!input) return true;

    TerminalSettings candidate;
    std::string line;
    while (std::getline(input, line)) {
        const auto delimiter = line.find('=');
        if (delimiter == std::string::npos) continue;
        const auto key = line.substr(0, delimiter);
        const auto value = line.substr(delimiter + 1);
        try {
            if (key == "width_pixels") candidate.width_pixels = std::stoi(value);
            else if (key == "height_pixels") candidate.height_pixels = std::stoi(value);
            else if (key == "resize_on_launch") candidate.resize_on_launch = value != "0";
            else if (key == "theme") candidate.theme = value;
            else if (key == "room_code") candidate.room_code = value;
            else if (key == "layout_mode") candidate.layout_mode = value;
            else if (key == "touch_keyboard") candidate.touch_keyboard = value != "0";
            else if (key == "companion_render") candidate.companion_render = value;
            else if (key == "sentinel_movie_speed") candidate.sentinel_movie_speed = std::stod(value);
            else if (key == "sentinel_last_will_enabled") candidate.sentinel_last_will_enabled = value != "0";
            else if (key == "java_companion" && value == "0") candidate.companion_render = "ansi";
        } catch (...) {
            error = "Invalid terminal setting: " + line;
            return false;
        }
    }
    if (!valid_terminal_resolution(candidate.width_pixels, candidate.height_pixels)) {
        error = "Terminal resolution must be between 640x480 and 3840x2160.";
        return false;
    }
    if (!valid_terminal_theme(candidate.theme)) {
        error = "Terminal theme must be blue, gold, or green.";
        return false;
    }
    if (!valid_terminal_layout(candidate.layout_mode)) {
        error = "Terminal layout must be workstation, minimal, or static.";
        return false;
    }
    if (!valid_companion_render(candidate.companion_render)) {
        error = "Companion render must be auto, ansi, or off.";
        return false;
    }
    if (!valid_sentinel_movie_speed(candidate.sentinel_movie_speed)) {
        error = "Sentinel movie speed must be 0.5, 1.0, or 2.0.";
        return false;
    }
    if (!valid_room_code(candidate.room_code)) {
        error = "Room code must be 1-16 letters, digits, '-' or '_'.";
        return false;
    }
    settings = candidate;
    return true;
}

bool save_terminal_settings(const TerminalSettings& settings, std::string& error) {
    if (!valid_terminal_resolution(settings.width_pixels, settings.height_pixels)) {
        error = "Terminal resolution must be between 640x480 and 3840x2160.";
        return false;
    }
    if (!valid_terminal_theme(settings.theme) ||
        !valid_terminal_layout(settings.layout_mode) ||
        !valid_companion_render(settings.companion_render) ||
        !valid_sentinel_movie_speed(settings.sentinel_movie_speed) ||
        !valid_room_code(settings.room_code)) {
        error = "Invalid terminal theme, layout, or room code.";
        return false;
    }
    std::error_code directory_error;
    std::filesystem::create_directories(settings_path().parent_path(), directory_error);
    if (directory_error) {
        error = "Could not create settings directory: " + directory_error.message();
        return false;
    }
    std::ofstream output(settings_path(), std::ios::trunc);
    if (!output) {
        error = "Could not write terminal settings: " + settings_path().string();
        return false;
    }
    output << "width_pixels=" << settings.width_pixels << '\n'
           << "height_pixels=" << settings.height_pixels << '\n'
           << "resize_on_launch=" << (settings.resize_on_launch ? 1 : 0) << '\n'
           << "theme=" << settings.theme << '\n'
           << "room_code=" << settings.room_code << '\n'
           << "layout_mode=" << settings.layout_mode << '\n'
           << "touch_keyboard=" << (settings.touch_keyboard ? 1 : 0) << '\n'
           << "companion_render=" << settings.companion_render << '\n'
           << "sentinel_movie_speed=" << settings.sentinel_movie_speed << '\n'
           << "sentinel_last_will_enabled="
           << (settings.sentinel_last_will_enabled ? 1 : 0) << '\n';
    return static_cast<bool>(output);
}

}  // namespace offgrid

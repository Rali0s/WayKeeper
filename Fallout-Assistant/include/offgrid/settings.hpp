#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace offgrid {

struct TerminalSettings {
    int width_pixels{640};
    int height_pixels{480};
    bool resize_on_launch{true};
    std::string theme{"blue"};
    std::string room_code{"WK-01"};
    std::string layout_mode{"workstation"};
    bool touch_keyboard{true};
    std::string companion_render{"off"};
    double sentinel_movie_speed{1.0};
    bool sentinel_last_will_enabled{false};
};

std::filesystem::path settings_path();
bool load_terminal_settings(TerminalSettings& settings, std::string& error);
bool save_terminal_settings(const TerminalSettings& settings, std::string& error);
bool valid_terminal_resolution(int width_pixels, int height_pixels);
bool valid_terminal_theme(std::string_view theme);
bool valid_terminal_layout(std::string_view layout);
bool valid_companion_render(std::string_view render);
bool valid_sentinel_movie_speed(double speed);
bool valid_room_code(std::string_view room_code);

}  // namespace offgrid

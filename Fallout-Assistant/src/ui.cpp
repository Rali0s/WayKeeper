#include "offgrid/ui.hpp"

#include "offgrid/guide.hpp"
#include "offgrid/herbs.hpp"
#include "offgrid/image.hpp"
#include "offgrid/inventory.hpp"
#include "offgrid/io_scout.hpp"
#include "offgrid/journal.hpp"
#include "offgrid/library.hpp"
#include "offgrid/map.hpp"
#include "offgrid/map_annotations.hpp"
#include "offgrid/mesh.hpp"
#include "offgrid/network.hpp"
#include "offgrid/ollama.hpp"
#include "offgrid/profile.hpp"
#include "offgrid/schematics.hpp"
#include "offgrid/sentinel.hpp"
#include "offgrid/settings.hpp"
#include "offgrid/terminal.hpp"
#include "offgrid/unlocks.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <iterator>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(__APPLE__) || defined(__unix__)
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace offgrid {
namespace {

// Large development displays keep the complete workstation. The shipping WK-01
// display is 640x480 and normally exposes roughly 80x24 terminal cells.
constexpr std::size_t width = 160;
constexpr std::size_t maximum_interface_width = 192;
constexpr std::size_t compact_display_columns = 120;
constexpr std::size_t compact_display_rows = 30;

namespace ansi {
constexpr const char* reset = "\033[0m";
constexpr const char* bold = "\033[1m";
const char* amber = "\033[38;5;39m";
const char* cyan = "\033[38;5;45m";
const char* green = "\033[38;5;82m";
constexpr const char* red = "\033[38;5;196m";
constexpr const char* white = "\033[38;5;255m";
constexpr const char* muted = "\033[38;5;245m";
constexpr const char* status_green_bar = "\033[1;38;5;16;48;5;82m";
constexpr const char* status_gold_bar = "\033[1;38;5;16;48;5;220m";
const char* small_green = "\033[2;38;5;75m";
const char* amber_bar = "\033[1;38;5;255;48;5;25m";
const char* cyan_bar = "\033[1;38;5;16;48;5;45m";
const char* boot_blue = "\033[38;5;39m";
const char* boot_gold = "\033[38;5;220m";
const char* boot_green = "\033[38;5;82m";
const char* boot_muted = "\033[38;5;75m";
const char* boot_bar = "\033[1;38;5;255;48;5;25m";
constexpr const char* boot_panel = "\033[48;5;233m";
}

TerminalSettings& ui_settings() {
    static TerminalSettings settings;
    return settings;
}

void apply_terminal_theme(const std::string_view theme) {
    if (theme == "gold") {
        ansi::amber = "\033[38;5;220m";
        ansi::cyan = "\033[38;5;214m";
        ansi::green = "\033[38;5;82m";
        ansi::small_green = "\033[2;38;5;143m";
        ansi::amber_bar = "\033[1;38;5;16;48;5;220m";
        ansi::cyan_bar = "\033[1;38;5;16;48;5;214m";
        ansi::boot_blue = "\033[38;5;220m";
        ansi::boot_muted = "\033[38;5;143m";
        ansi::boot_bar = "\033[1;38;5;220;48;5;22m";
    } else if (theme == "green") {
        ansi::amber = "\033[38;5;82m";
        ansi::cyan = "\033[38;5;120m";
        ansi::green = "\033[38;5;46m";
        ansi::small_green = "\033[2;38;5;71m";
        ansi::amber_bar = "\033[1;38;5;16;48;5;82m";
        ansi::cyan_bar = "\033[1;38;5;16;48;5;120m";
        ansi::boot_blue = "\033[38;5;82m";
        ansi::boot_muted = "\033[38;5;71m";
        ansi::boot_bar = "\033[1;38;5;82;48;5;22m";
    } else {
        ansi::amber = "\033[38;5;39m";
        ansi::cyan = "\033[38;5;45m";
        ansi::green = "\033[38;5;82m";
        ansi::small_green = "\033[2;38;5;75m";
        ansi::amber_bar = "\033[1;38;5;255;48;5;25m";
        ansi::cyan_bar = "\033[1;38;5;16;48;5;45m";
        ansi::boot_blue = "\033[38;5;39m";
        ansi::boot_muted = "\033[38;5;75m";
        ansi::boot_bar = "\033[1;38;5;255;48;5;25m";
    }
}

struct HostBattery {
    std::optional<int> percent;
    std::string state{"EXT"};
};

HostBattery host_battery() {
    HostBattery result;
    if (const char* configured = std::getenv("WAYKEEPER_BATTERY_PERCENT")) {
        try {
            result.percent = std::clamp(std::stoi(configured), 0, 100);
            result.state = "SENSOR";
            return result;
        } catch (...) {}
    }
#ifndef _WIN32
    const std::filesystem::path supplies{"/sys/class/power_supply"};
    std::error_code error;
    if (!std::filesystem::is_directory(supplies, error)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(supplies, error)) {
        std::ifstream capacity(entry.path() / "capacity");
        int percent = 0;
        if (!(capacity >> percent)) continue;
        result.percent = std::clamp(percent, 0, 100);
        std::ifstream status(entry.path() / "status");
        std::getline(status, result.state);
        if (result.state.empty()) result.state = "BAT";
        break;
    }
#endif
    return result;
}

std::string battery_rail(const HostBattery& battery) {
    if (!battery.percent) return "BAT [----------] EXT";
    const auto cells = static_cast<std::size_t>((*battery.percent + 9) / 10);
    return "BAT [" + std::string(cells, '#') + std::string(10 - cells, '-') + "] " +
        std::to_string(*battery.percent) + "% " + battery.state;
}

void paint(const bool enabled, const char* code) {
    if (enabled) std::cout << code;
}

void clear(const bool enabled) {
    if (enabled) std::cout << "\033[2J\033[H";
    else std::cout << "\n\n";
}

void clear_launch_window(const bool enabled) {
    if (enabled) std::cout << "\033[3J\033[2J\033[H" << std::flush;
    else std::cout << std::string(60, '\n') << std::flush;
}

class FixedTerminalScreen {
public:
    explicit FixedTerminalScreen(const bool enabled) : enabled_(enabled) {
        if (enabled_) {
            // Alternate screen keeps WayKeeper out of the terminal's scrollback.
            // No-wrap prevents an accidental long cell from pushing the board.
            std::cout << "\033[?1049h\033[?7l\033[2J\033[H" << std::flush;
        }
    }

    ~FixedTerminalScreen() {
        if (enabled_) {
            std::cout << "\033[0m\033[?7h\033[?1049l" << std::flush;
        }
    }

    FixedTerminalScreen(const FixedTerminalScreen&) = delete;
    FixedTerminalScreen& operator=(const FixedTerminalScreen&) = delete;

private:
    bool enabled_{};
};

void set_title(const bool enabled) {
    if (enabled) std::cout << "\033]0;WAYKEEPER // Off-Grid Operations Terminal\007";
}

std::size_t interface_width() {
    const auto columns = terminal_size().columns;
    const auto available = columns > 1 ? columns - 1 : columns;
    if (columns < compact_display_columns || terminal_size().rows < compact_display_rows) {
        return std::clamp<std::size_t>(available, 60, 119);
    }
    if (ui_settings().layout_mode == "minimal")
        return std::clamp<std::size_t>(available, 80, 120);
    if (columns <= width) return width;
    return std::clamp<std::size_t>(columns - 1, width, maximum_interface_width);
}

bool compact_display() {
    const auto terminal = terminal_size();
    return terminal.columns < compact_display_columns || terminal.rows < compact_display_rows;
}

bool appliance_mode() {
#if WAYKEEPER_APPLIANCE_MODE
    return true;
#else
    const char* requested = std::getenv("WAYKEEPER_APPLIANCE_MODE");
    return requested && (std::string_view(requested) == "1" ||
                         std::string_view(requested) == "true" ||
                         std::string_view(requested) == "on");
#endif
}

void rule(char character = '-') {
    std::cout << std::string(interface_width(), character) << '\n';
}

void expanded_line(const std::string_view value, const std::size_t line_width = 0) {
    const auto target = line_width == 0 ? interface_width() : line_width;
    if (value.size() > target) {
        if (target > 3) std::cout << value.substr(0, target - 3) << "...";
        else std::cout << value.substr(0, target);
    } else {
        std::cout << value << std::string(target - value.size(), ' ');
    }
    std::cout << '\n';
}

void finish_aligned_line(const std::size_t visible_width) {
    const auto target = interface_width();
    if (visible_width < target) std::cout << std::string(target - visible_width, ' ');
    std::cout << '\n';
}

void centered_line(const std::string_view text, const std::size_t line_width = 0) {
    const auto target = line_width == 0 ? interface_width() : line_width;
    const auto padding = text.size() < target ? (target - text.size()) / 2 : 0;
    std::cout << std::string(padding, ' ') << text << '\n';
}

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string compact_slashes(std::string value) {
    for (auto position = value.find(" / "); position != std::string::npos;
         position = value.find(" / ", position + 1)) {
        value.replace(position, 3, "/");
    }
    return value;
}

std::string normalize_shell_command(const std::string& value) {
    const auto escape = value.find('\x1b');
    const std::string tail = escape == std::string::npos ? value : value.substr(escape + 1);
    if (tail == "OP" || tail == "[11~") return "f1";
    if (tail == "OQ" || tail == "[12~") return "f2";
    if (tail == "OR" || tail == "[13~") return "f3";
    if (tail == "OS" || tail == "[14~") return "f4";
    if (tail == "[15~") return "f5";
    if (tail == "[17~") return "f6";
    if (tail == "[18~") return "f7";
    if (tail == "[19~") return "f8";
    if (tail == "[20~") return "f9";
    if (tail == "[21~") return "f10";
    if (tail == "[23~") return "f11";
    if (tail == "[24~") return "f12";
    if (tail == "[5~") return "pgup";
    if (tail == "[6~") return "pgdn";
    if (tail == "[A" || tail == "OA") return "up";
    if (tail == "[B" || tail == "OB") return "down";
    if (tail == "[C" || tail == "OC") return "right";
    if (tail == "[D" || tail == "OD") return "left";
    if (tail == "[H" || tail == "OH" || tail == "[1~") return "home";
    if (tail == "[F" || tail == "OF" || tail == "[4~") return "end";
    if (tail == "[Z") return "backtab";
    if (value == "\x1b") return "escape";
    if (value == "\t") return "tab";
    if (value == "\x02") return "pgup";
    if (value == "\x06") return "pgdn";
    if (value == "\f") return "clear";
    return lower(value);
}

struct IdleLockSignal {};

class DeadManController {
public:
    void arm() {
        armed_ = true;
        activity();
    }

    void disarm() { armed_ = false; }
    bool armed() const { return armed_; }
    void activity() { last_activity_ = std::chrono::steady_clock::now(); }

    std::chrono::milliseconds remaining() const {
        if (!armed_) return std::chrono::milliseconds::max();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_activity_);
        return std::max(
            std::chrono::milliseconds{0},
            std::chrono::seconds{sentinel_idle_timeout_seconds} - elapsed);
    }

private:
    bool armed_{};
    std::chrono::steady_clock::time_point last_activity_{std::chrono::steady_clock::now()};
};

DeadManController& deadman_controller() {
    static DeadManController controller;
    return controller;
}

std::string movie_speed_label() {
    std::ostringstream label;
    label << std::fixed << std::setprecision(1) << ui_settings().sentinel_movie_speed << 'X';
    return label.str();
}

std::string deadman_prompt_suffix() {
    const auto remaining = deadman_controller().remaining();
    const auto total_seconds = static_cast<long long>((remaining.count() + 999) / 1000);
    const auto minutes = total_seconds / 60;
    const auto seconds = total_seconds % 60;
    std::ostringstream suffix;
    suffix << "  [LOCK " << std::setfill('0') << std::setw(2) << minutes << ':'
           << std::setw(2) << seconds << " | MOVIE " << movie_speed_label() << ']';
    return suffix.str();
}

void redraw_timed_prompt(
    const std::string_view label, const std::string_view value, const bool ansi_enabled) {
    if (ansi_enabled) std::cout << "\r\033[2K";
    else std::cout << '\r';
    paint(ansi_enabled, ansi::amber);
    std::cout << label;
    paint(ansi_enabled, ansi::reset);
    std::cout << value;
    paint(ansi_enabled, ansi::muted);
    std::cout << deadman_prompt_suffix();
    paint(ansi_enabled, ansi::reset);
    std::cout << std::flush;
}

TerminalKeyResult timed_terminal_key(
    const std::string_view label, const std::string_view value,
    const bool ansi_enabled) {
    auto& deadman = deadman_controller();
    for (;;) {
        const auto remaining = deadman.remaining();
        if (remaining <= std::chrono::milliseconds{0}) throw IdleLockSignal{};
        redraw_timed_prompt(label, value, ansi_enabled);
        const auto wait = std::min(remaining, std::chrono::milliseconds{1000});
        auto result = read_terminal_key_for(wait);
        if (result.status == TerminalKeyStatus::Timeout) continue;
        if (result.status == TerminalKeyStatus::Key) deadman.activity();
        return result;
    }
}

std::string timed_line_prompt(
    const std::string_view label, std::string value, const bool ansi_enabled) {
    if (!deadman_controller().armed()) {
        paint(ansi_enabled, ansi::amber);
        std::cout << label;
        paint(ansi_enabled, ansi::reset);
        std::cout << value << std::flush;
        std::string tail;
        std::getline(std::cin, tail);
        return value + tail;
    }

    for (;;) {
        const auto result = timed_terminal_key(label, value, ansi_enabled);
        if (result.status == TerminalKeyStatus::Unavailable) {
            std::string tail;
            std::getline(std::cin, tail);
            deadman_controller().activity();
            return value + tail;
        }
        if (result.status == TerminalKeyStatus::EndOfInput) {
            std::cout << '\n';
            return value;
        }
        if (result.key.empty()) {
            std::cout << '\n';
            return value;
        }
        if (result.key == "\x7f" || result.key == "\b") {
            if (!value.empty()) value.pop_back();
            continue;
        }
        if (result.key == "escape") {
            value.clear();
            std::cout << '\n';
            return value;
        }
        if (result.key.size() == 1 &&
            std::isprint(static_cast<unsigned char>(result.key.front()))) {
            value.push_back(result.key.front());
        }
    }
}

std::string prompt(std::string_view label, const bool ansi_enabled = false) {
    return timed_line_prompt(label, {}, ansi_enabled);
}

std::string prompt_seeded(
    std::string_view label, const std::string_view seed, const bool ansi_enabled = false) {
    return timed_line_prompt(label, std::string(seed), ansi_enabled);
}

bool accepted_navigation_key(const std::string_view command) {
    if (command.empty() || command == "tab" || command == "backtab" ||
        command == "escape" || command == "up" || command == "down" ||
        command == "left" || command == "right" || command == "home" ||
        command == "end" || command == "pgup" || command == "pgdn") {
        return true;
    }
    if (command.size() == 1 && std::isdigit(
            static_cast<unsigned char>(command.front()))) {
        return true;
    }
    if (command.size() >= 2 && command.front() == 'f') {
        try {
            const auto key = std::stoi(std::string(command.substr(1)));
            return key >= 1 && key <= 12;
        } catch (...) {}
    }
    return false;
}

std::string pager_prompt(
    std::string_view label, const bool ansi_enabled,
    const bool navigation_lock = false) {
    for (;;) {
        if (deadman_controller().armed()) {
            const auto result = timed_terminal_key(label, {}, ansi_enabled);
            if (result.status == TerminalKeyStatus::Key) {
                if (ansi_enabled) std::cout << "\r\033[2K";
                const auto command = normalize_shell_command(result.key);
                if (!ansi_enabled || !navigation_lock || accepted_navigation_key(command)) {
                    return command;
                }
                continue;
            }
            if (result.status == TerminalKeyStatus::EndOfInput) return "q";
        }
        paint(ansi_enabled, ansi::amber);
        std::cout << label;
        paint(ansi_enabled, ansi::reset);
        std::cout << std::flush;
        if (const auto key = read_pager_key()) {
            const auto command = normalize_shell_command(*key);
            if (!ansi_enabled || !navigation_lock || accepted_navigation_key(command)) {
                return command;
            }
            continue;
        }
        std::string value;
        std::getline(std::cin, value);
        return normalize_shell_command(value);
    }
}

void pause(const bool ansi_enabled) {
    prompt("\n[ RETURN ]", ansi_enabled);
}

struct PagerGeometry {
    std::size_t columns;
    std::size_t rows;
};

PagerGeometry pager_geometry() {
    const auto reserved_rows = compact_display() ? std::size_t{9} : std::size_t{16};
    const auto terminal = terminal_size();
    const auto available_rows = terminal.rows > reserved_rows ? terminal.rows - reserved_rows : 6;
    const auto available_columns = terminal.columns > 1 ? terminal.columns - 1 : terminal.columns;
    return {
        std::clamp<std::size_t>(available_columns, 20, width),
        std::max<std::size_t>(6, available_rows),
    };
}

std::string expand_tabs(std::string_view line) {
    std::string expanded;
    expanded.reserve(line.size());
    for (const char character : line) {
        if (character == '\r') continue;
        if (character != '\t') {
            expanded.push_back(character);
            continue;
        }
        const auto spaces = 4 - (expanded.size() % 4);
        expanded.append(spaces, ' ');
    }
    return expanded;
}

std::vector<std::string> pager_lines(std::string_view text, const std::size_t line_width) {
    std::vector<std::string> lines;
    std::istringstream input{std::string(text)};
    std::string raw_line;
    while (std::getline(input, raw_line)) {
        auto line = expand_tabs(raw_line);
        if (line.empty()) {
            lines.emplace_back();
            continue;
        }
        while (line.size() > line_width) {
            auto split = line.rfind(' ', line_width);
            if (split == std::string::npos || split < line_width / 2) split = line_width;
            lines.push_back(line.substr(0, split));
            auto next = split;
            while (next < line.size() && line[next] == ' ') ++next;
            line.erase(0, next);
        }
        lines.push_back(std::move(line));
    }
    if (lines.empty()) lines.emplace_back("PAGE TEXT UNAVAILABLE - OPEN THE ORIGINAL SOURCE IF PRESENT.");
    return lines;
}

std::size_t pager_max_offset(
    const std::vector<std::string>& lines, const std::size_t viewport_rows) {
    return lines.size() > viewport_rows ? lines.size() - viewport_rows : 0;
}

void render_pager(
    const std::vector<std::string>& lines, std::size_t& offset,
    const PagerGeometry geometry, const bool ansi_enabled) {
    offset = std::min(offset, pager_max_offset(lines, geometry.rows));
    const auto end = std::min(lines.size(), offset + geometry.rows);
    for (auto index = offset; index < end; ++index) std::cout << lines[index] << '\n';
    for (auto index = end - offset; index < geometry.rows; ++index) std::cout << '\n';

    rule();
    const auto percent = lines.empty() ? 100 : std::min<std::size_t>(
        100, ((end == 0 ? 0 : end) * 100) / lines.size());
    paint(ansi_enabled, ansi::cyan);
    std::cout << "VIEW  LINES " << (lines.empty() ? 0 : offset + 1) << '-' << end
              << " / " << lines.size() << "  |  " << percent << "%\n";
    paint(ansi_enabled, ansi::muted);
    std::cout << "SCROLL  SPACE/PGDN  B/PGUP  J/K LINE  D/U HALF  GG/HOME  G/END\n"
                 "SOURCE  N/P PAGE  I IMAGE (PDF)  O OPEN SOURCE  Q BACK  |  :GOTO <PAGE>\n";
    paint(ansi_enabled, ansi::reset);
}

void render_vim_text(
    const std::vector<std::string>& lines, std::size_t& offset,
    const PagerGeometry geometry, const bool ansi_enabled) {
    offset = std::min(offset, pager_max_offset(lines, geometry.rows));
    const auto end = std::min(lines.size(), offset + geometry.rows);
    for (auto index = offset; index < end; ++index) std::cout << lines[index] << '\n';
    for (auto row = end - offset; row < geometry.rows; ++row) std::cout << '\n';
    rule();
    const auto percent = lines.empty() ? 100 : std::min<std::size_t>(
        100, ((end == 0 ? 0 : end) * 100) / lines.size());
    paint(ansi_enabled, ansi::cyan);
    std::cout << "FIXED READER  |  LINES " << (lines.empty() ? 0 : offset + 1)
              << '-' << end << " / " << lines.size() << "  |  " << percent << "%\n";
    paint(ansi_enabled, ansi::muted);
    std::cout << "SCROLL  UP/DOWN | PGUP/PGDN | HOME/END | ESC BACK\n";
    paint(ansi_enabled, ansi::reset);
}

bool pager_scroll(
    const std::string& command, std::size_t& offset,
    const std::vector<std::string>& lines, const std::size_t viewport_rows) {
    const auto maximum = pager_max_offset(lines, viewport_rows);
    if (command == "j" || command == "down" || command.empty()) {
        offset = std::min(maximum, offset + 1);
        return true;
    }
    if (command == "k" || command == "up") {
        offset = offset > 0 ? offset - 1 : 0;
        return true;
    }
    if (command == "d") {
        offset = std::min(maximum, offset + std::max<std::size_t>(1, viewport_rows / 2));
        return true;
    }
    if (command == "u") {
        const auto distance = std::max<std::size_t>(1, viewport_rows / 2);
        offset = offset > distance ? offset - distance : 0;
        return true;
    }
    if (command == "gg" || command == "home" || command == "top") {
        offset = 0;
        return true;
    }
    if (command == "g" || command == "end" || command == "bottom") {
        offset = maximum;
        return true;
    }
    return false;
}

struct VimMenuState {
    std::size_t selected{};
    std::size_t offset{};
};

std::size_t vim_menu_rows(const std::size_t reserved_rows = 13) {
    const auto terminal = terminal_size();
    const auto available = terminal.rows > reserved_rows ? terminal.rows - reserved_rows : 4;
    return std::clamp<std::size_t>(available, 4, 25);
}

void clamp_vim_menu(
    VimMenuState& state, const std::size_t count, const std::size_t visible_rows) {
    if (count == 0) {
        state = {};
        return;
    }
    state.selected = std::min(state.selected, count - 1);
    if (state.selected < state.offset) state.offset = state.selected;
    if (state.selected >= state.offset + visible_rows) {
        state.offset = state.selected - visible_rows + 1;
    }
    state.offset = std::min(state.offset, count > visible_rows ? count - visible_rows : 0);
}

void render_vim_menu(
    const std::vector<std::string>& items, VimMenuState& state,
    const std::size_t visible_rows, const bool ansi_enabled, const bool pad_rows = true) {
    clamp_vim_menu(state, items.size(), visible_rows);
    const auto end = std::min(items.size(), state.offset + visible_rows);
    const auto columns = std::max<std::size_t>(20, std::min(width, terminal_size().columns));
    for (auto index = state.offset; index < end; ++index) {
        auto item = items[index];
        const auto item_width = columns > 8 ? columns - 8 : columns;
        if (item.size() > item_width) item = item.substr(0, item_width - 3) + "...";
        if (index == state.selected) {
            paint(ansi_enabled, ansi::cyan_bar);
            std::cout << "> [" << std::setw(2) << index + 1 << "] " << item;
            paint(ansi_enabled, ansi::reset);
            std::cout << '\n';
        } else {
            std::cout << "  [" << std::setw(2) << index + 1 << "] " << item << '\n';
        }
    }
    if (pad_rows) {
        for (auto row = end - state.offset; row < visible_rows; ++row) std::cout << '\n';
    }
    rule();
    paint(ansi_enabled, ansi::cyan);
    if (items.empty()) {
        std::cout << "INPUT LOCK  |  NO ITEMS\n";
    } else {
        std::cout << "INPUT LOCK  |  ITEM " << state.selected + 1 << " / " << items.size()
                  << "  |  VIEW " << state.offset + 1 << '-' << end << '\n';
    }
    paint(ansi_enabled, ansi::muted);
    std::cout << "INPUT LOCK  UP/DOWN | PGUP/PGDN | HOME/END | ENTER | 0-9 | ESC\n";
    paint(ansi_enabled, ansi::reset);
}

bool navigate_vim_menu(
    const std::string& command, VimMenuState& state,
    const std::size_t count, const std::size_t visible_rows) {
    if (count == 0) return false;
    if (command == "down" || command == "j") {
        state.selected = std::min(count - 1, state.selected + 1);
    } else if (command == "up" || command == "k") {
        state.selected = state.selected > 0 ? state.selected - 1 : 0;
    } else if (command == " " || command == "space" || command == "pgdn" || command == "d") {
        state.selected = std::min(count - 1, state.selected + visible_rows);
    } else if (command == "b" || command == "pgup" || command == "u") {
        state.selected = state.selected > visible_rows ? state.selected - visible_rows : 0;
    } else if (command == "gg" || command == "home" || command == "top") {
        state.selected = 0;
    } else if (command == "g" || command == "end" || command == "bottom") {
        state.selected = count - 1;
    } else {
        return false;
    }
    clamp_vim_menu(state, count, visible_rows);
    return true;
}

std::optional<std::size_t> vim_menu_choice(
    std::string command, const VimMenuState& state, const std::size_t count) {
    if (count == 0) return std::nullopt;
    if (command.empty() || command == "enter" || command == "open" || command == "o") {
        return state.selected;
    }
    if (command.rfind("open ", 0) == 0) command = command.substr(5);
    try {
        const auto selected = static_cast<std::size_t>(std::stoull(command));
        if (selected == 0 && count >= 10) return 9;
        if (selected > 0 && selected <= count) return selected - 1;
    } catch (...) {}
    return std::nullopt;
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::ostringstream formatted;
    formatted << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return formatted.str();
}

void logo(const bool ansi_enabled) {
    clear(ansi_enabled);
    paint(ansi_enabled, ansi::amber);
    std::cout << R"LOGO(
__        ___ __   _____  _______ _____ ____  _____ ____
\ \      / / / \ \ / /  |/ / ____| ____|  _ \| ____|  _ \
 \ \ /\ / / _ \ \ V /| ' /|  _| |  _| | |_) |  _| | |_) |
  \ V  V / ___ \| | | . \| |___| |___|  __/| |___|  _ <
   \_/\_/_/   \_\_| |_|\_\_____|_____|_|   |_____|_| \_\

             O F F - G R I D   K N O W L E D G E   T E R M I N A L
)LOGO";
    paint(ansi_enabled, ansi::cyan);
    std::cout << "        PERSONAL FIELD TERMINAL // LOCAL-FIRST // NO CLOUD REQUIRED\n";
    paint(ansi_enabled, ansi::reset);
    rule('=');
}

std::filesystem::path mascot_asset(
    const std::filesystem::path& root, std::string_view incident);

void splash_screen(const bool ansi_enabled) {
    const auto panel_width = interface_width();
    const auto color_mode = terminal_color_mode(ansi_enabled);
    if (compact_display()) {
        const auto panel_line = [&](std::string value, const char* color) {
            const auto inner = panel_width > 4 ? panel_width - 4 : panel_width;
            if (value.size() > inner) value.resize(inner);
            value.append(inner - value.size(), ' ');
            paint(ansi_enabled, ansi::boot_blue);
            std::cout << "| ";
            paint(ansi_enabled, color);
            std::cout << value;
            paint(ansi_enabled, ansi::boot_blue);
            std::cout << " |\n";
        };
        clear(ansi_enabled);
        paint(ansi_enabled, ansi::boot_blue);
        std::cout << '+' << std::string(panel_width - 2, '=') << "+\n|";
        paint(ansi_enabled, ansi::boot_bar);
        std::string compact_title = " WAYKEEPER(TM) // OFF-GRID KNOWLEDGE TERMINAL ";
        if (compact_title.size() > panel_width - 2) compact_title.resize(panel_width - 2);
        compact_title.append(panel_width - 2 - compact_title.size(), ' ');
        std::cout << compact_title;
        paint(ansi_enabled, ansi::boot_blue);
        std::cout << "|\n+" << std::string(panel_width - 2, '-') << "+\n";
        panel_line("WK-01 // SURVIVAL OPERATING SYSTEM", ansi::boot_gold);
        panel_line("LOCAL-FIRST // AIR-GAPPED CAPABLE", ansi::boot_blue);
        panel_line("ROOM CODE ..................... [" + ui_settings().room_code + "]", ansi::boot_blue);
        panel_line("CORE SERVICES", ansi::boot_gold);
        panel_line("  ARCHIVE ...................... [VERIFIED]", ansi::boot_green);
        panel_line("  NAVIGATION ................... [STANDBY]", ansi::boot_green);
        panel_line("  MEDICAL INDEX ................ [READY]", ansi::boot_green);
        panel_line("  SIGNAL SUITE ................. [LISTENING]", ansi::boot_green);
        panel_line("  ENVIRONMENT .................. [UNKNOWN]", ansi::boot_green);
        panel_line("", ansi::boot_blue);
        panel_line("DISPLAY  640X480 LCD // ANSI 256 // FIXED SCREEN", ansi::boot_gold);
        panel_line("SYSTEM LOAD ........ [################] 100%", ansi::boot_blue);
        panel_line("DIRECTIVE  SURVIVE // PRESERVE // REBUILD", ansi::boot_blue);
        panel_line("THE WORLD MAY BE LOST. YOU ARE NOT.", ansi::boot_blue);
        paint(ansi_enabled, ansi::boot_blue);
        std::cout << '+' << std::string(panel_width - 2, '=') << "+\n";
        paint(ansi_enabled, ansi::boot_gold);
        expanded_line("[ PRESS ENTER ]  OPEN WAYKEEPER", panel_width);
        paint(ansi_enabled, ansi::reset);
        (void)pager_prompt("", ansi_enabled, true);
        return;
    }
    const auto image_width = std::clamp<std::size_t>(panel_width / 3, 42, 58);
    const auto left_width = panel_width - image_width - 5;
    const auto available_rows = terminal_size().rows > 14 ? terminal_size().rows - 14 : 18;
    const auto image_rows = std::min<std::size_t>(
        std::clamp<std::size_t>(available_rows, 18, 28),
        std::max<std::size_t>(18, static_cast<std::size_t>(image_width / 1.68)));
    const auto rendered_image_width = std::min<std::size_t>(
        image_width, static_cast<std::size_t>(std::lround(image_rows * 1.68)));
    const auto image_left_padding = (image_width - rendered_image_width) / 2;
    const auto image_right_padding = image_width - rendered_image_width - image_left_padding;
    const std::string color_label = color_mode == TerminalColorMode::Ansi256
        ? "[ANSI 256 / 8-BIT]" : color_mode == TerminalColorMode::TrueColor
            ? "[TRUECOLOR RGB]" : "[MONOCHROME]";
    std::vector<std::string> status{
        "WK-01 // SURVIVAL OPERATING SYSTEM",
        "LOCAL-FIRST // AIR-GAPPED CAPABLE",
        "ROOM CODE ..................... [" + ui_settings().room_code + "]",
        "CORE SERVICES",
        "  ARCHIVE ...................... [VERIFIED]",
        "  NAVIGATION ................... [STANDBY]",
        "  MEDICAL INDEX ................ [READY]",
        "  SIGNAL SUITE ................. [LISTENING]",
        "  ENVIRONMENT .................. [UNKNOWN]",
        "",
        "DISPLAY PIPELINE",
        "  COLOR ......................... " + color_label,
        "  ANSI DASHBOARD ................ [256 COLOR]",
        "  PIXEL CELL .................... [HALF BLOCK 2:1]",
        "",
        "SYSTEM LOAD ........ [################] 100%",
        "DIRECTIVE  SURVIVE // PRESERVE // REBUILD",
        "THE WORLD MAY BE LOST. YOU ARE NOT.",
    };
    status.resize(image_rows);

    std::vector<std::string> image_lines;
    const auto asset = mascot_asset(resource_root(), "");
    if (!asset.empty() && image_support_available()) {
        std::ostringstream rendered;
        ImageInfo image_info;
        std::string image_error;
        if (render_image_ansi(
                asset, rendered_image_width, image_rows, ansi_enabled,
                rendered, image_info, image_error)) {
            std::istringstream stream(rendered.str());
            std::string line;
            while (std::getline(stream, line)) image_lines.push_back(std::move(line));
        }
    }
    while (image_lines.size() < image_rows) {
        image_lines.emplace_back(rendered_image_width, ' ');
    }

    const auto padded = [](std::string value, const std::size_t size) {
        if (value.size() > size) value.resize(size);
        value.append(size - value.size(), ' ');
        return value;
    };

    clear(ansi_enabled);
    paint(ansi_enabled, ansi::boot_blue);
    std::cout << '+' << std::string(left_width, '=') << '+'
              << std::string(image_width + 2, '=') << "+\n";
    const std::string title_left = " WAYKEEPER(TM) // OFF-GRID KNOWLEDGE TERMINAL";
    const std::string title_right = "ROOM: " + ui_settings().room_code + " ";
    const auto title_inner_width = panel_width - 2;
    paint(ansi_enabled, ansi::boot_blue);
    std::cout << '|';
    paint(ansi_enabled, ansi::boot_bar);
    std::cout << title_left
              << std::string(title_inner_width - title_left.size() - title_right.size(), ' ')
              << title_right;
    paint(ansi_enabled, ansi::boot_blue);
    std::cout << '|';
    paint(ansi_enabled, ansi::reset);
    std::cout << '\n';
    paint(ansi_enabled, ansi::boot_blue);
    std::cout << '+' << std::string(left_width, '-') << '+'
              << std::string(image_width + 2, '-') << "+\n";

    for (std::size_t row = 0; row < image_rows; ++row) {
        paint(ansi_enabled, ansi::boot_blue);
        std::cout << '|';
        paint(ansi_enabled, ansi::boot_panel);
        const char* status_color = ansi::boot_muted;
        if (row == 0 || row == 3 || row == 10) status_color = ansi::boot_gold;
        else if (row >= 4 && row <= 8) status_color = ansi::boot_green;
        else if (row >= 15) status_color = ansi::boot_blue;
        paint(ansi_enabled, status_color);
        std::cout << ' ' << padded(status[row], left_width - 2) << ' ';
        paint(ansi_enabled, ansi::reset);
        paint(ansi_enabled, ansi::boot_blue);
        std::cout << '|' << std::string(image_left_padding + 1, ' ')
                  << image_lines[row] << std::string(image_right_padding + 1, ' ');
        paint(ansi_enabled, ansi::boot_blue);
        std::cout << "|\n";
    }

    std::cout << '+' << std::string(left_width, '-') << '+'
              << std::string(image_width + 2, '-') << "+\n";
    paint(ansi_enabled, ansi::boot_gold);
    const std::string launch_status = "  [ PRESS ENTER ]  OPEN WAYKEEPER  //  " +
        std::string(color_mode == TerminalColorMode::Ansi256
            ? "8-BIT MASCOT PIPELINE ONLINE_"
            : color_mode == TerminalColorMode::TrueColor
                ? "TRUECOLOR MASCOT PIPELINE ONLINE_"
                : "MONOCHROME MASCOT PIPELINE ONLINE_");
    expanded_line(launch_status, panel_width);
    paint(ansi_enabled, ansi::reset);
    (void)pager_prompt("", ansi_enabled, true);
}

std::filesystem::path mascot_asset(
    const std::filesystem::path& root, const std::string_view incident) {
    std::string filename = "SurvivalMode.png";
    if (incident.find("Nuclear") != std::string_view::npos ||
        incident.find("Radiological") != std::string_view::npos) {
        filename = "VaultTec-Blue.png";
    } else if (incident.find("Zombie") != std::string_view::npos) {
        filename = "Zombie-FalloutMode.png";
    }
    for (const auto& directory : {
             root / "RES" / "WayKeeper TM", root.parent_path() / "RES" / "WayKeeper TM"}) {
        const auto candidate = directory / filename;
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

const std::vector<std::string>& sentinel_mascot_lines(
    const std::filesystem::path& root, const bool ansi_enabled,
    const std::size_t columns, const std::size_t rows) {
    struct Cache {
        std::filesystem::path asset;
        TerminalColorMode color_mode{TerminalColorMode::Plain};
        std::size_t columns{};
        std::size_t rows{};
        std::vector<std::string> lines;
    };
    static Cache cache;
    const auto asset = mascot_asset(root, "");
    const auto color_mode = terminal_color_mode(ansi_enabled);
    if (cache.asset == asset && cache.color_mode == color_mode &&
        cache.columns == columns && cache.rows == rows && !cache.lines.empty()) {
        return cache.lines;
    }

    cache = {};
    cache.asset = asset;
    cache.color_mode = color_mode;
    cache.columns = columns;
    cache.rows = rows;
    if (!asset.empty() && image_support_available()) {
        std::ostringstream rendered;
        ImageInfo info;
        std::string error;
        if (render_image_ansi(
                asset, columns, rows, ansi_enabled, rendered, info, error)) {
            std::istringstream stream(rendered.str());
            std::string line;
            while (std::getline(stream, line)) cache.lines.push_back(std::move(line));
        }
    }
    while (cache.lines.size() < rows) cache.lines.emplace_back(columns, ' ');
    return cache.lines;
}

std::filesystem::path sentinel_last_will_path() {
    return settings_path().parent_path() / "last-will.txt";
}

void render_sentinel_idle_frame(
    const std::filesystem::path& root, const SentinelMode mode,
    const std::size_t card, const bool ansi_enabled) {
    static constexpr std::array<std::array<std::string_view, 3>, 3> cards{{
        {{"FIELD UNIT STANDING BY", "PRIVATE ARCHIVE SEALED", "OPERATOR NEARBY"}},
        {{"WAYKEEPER SENTINEL ACTIVE", "PUBLIC DISPLAY // ZERO PRIVATE DATA", "SURVIVE // PRESERVE // REBUILD"}},
        {{"TRAVELER TERMINAL AT REST", "NO LOCATION OR INVENTORY BROADCAST", "ROOM CODE AVAILABLE FOR IDENTIFICATION"}},
    }};

    const auto panel_width = interface_width();
    std::string will_error;
    const auto will_message = ui_settings().sentinel_last_will_enabled
        ? load_sentinel_last_will(sentinel_last_will_path(), will_error)
        : std::nullopt;
    const auto will_width = panel_width > 12 ? panel_width - 12 : panel_width;
    const auto will_rows = std::clamp<std::size_t>(
        terminal_size().rows > 13 ? terminal_size().rows - 13 : 6, 6, 20);
    const auto will_lines = will_message
        ? pager_lines(*will_message, will_width) : std::vector<std::string>{};
    const auto will_pages = will_lines.empty()
        ? std::size_t{} : (will_lines.size() + will_rows - 1) / will_rows;
    const auto base_cards = mode == SentinelMode::Sentinel
        ? cards.size() : mode == SentinelMode::Quiet ? std::size_t{1} : std::size_t{};
    const auto public_cards = base_cards +
        (mode == SentinelMode::Blackout ? std::size_t{} : will_pages);
    const auto active_card = public_cards > 0 ? card % public_cards : std::size_t{};
    const bool show_last_will = mode != SentinelMode::Blackout &&
        active_card >= base_cards && will_pages > 0;
    clear(ansi_enabled);
    if (ansi_enabled) {
        std::cout << "\033]0;WAYKEEPER // PUBLIC IDLE\007";
    }
    paint(ansi_enabled, mode == SentinelMode::Blackout ? ansi::muted : ansi::boot_blue);
    std::cout << '+' << std::string(panel_width - 2, '=') << "+\n";
    const auto title = "WAYKEEPER // " + std::string(sentinel_mode_name(mode)) + " // PUBLIC IDLE";
    centered_line(title);
    std::cout << '+' << std::string(panel_width - 2, '-') << "+\n";

    if (mode == SentinelMode::Blackout) {
        const auto rows = std::max<std::size_t>(8, terminal_size().rows > 9
            ? terminal_size().rows - 9 : 8);
        for (std::size_t row = 0; row < rows / 2; ++row) std::cout << '\n';
        paint(ansi_enabled, ansi::muted);
        centered_line("SEALED FIELD TERMINAL");
        centered_line("ROOM // " + ui_settings().room_code);
        for (std::size_t row = rows / 2; row + 2 < rows; ++row) std::cout << '\n';
    } else if (show_last_will) {
        const auto page = active_card - base_cards;
        paint(ansi_enabled, ansi::boot_gold);
        centered_line("LAST WILL & TESTAMENT // IF THIS WAYKEEPER IS FOUND");
        paint(ansi_enabled, ansi::muted);
        centered_line("VOLUNTARILY PUBLISHED FOUND-DEVICE MESSAGE // ROOM " +
            ui_settings().room_code);
        std::cout << '\n';
        const auto start = page * will_rows;
        const auto end = std::min(will_lines.size(), start + will_rows);
        for (auto index = start; index < end; ++index) {
            paint(ansi_enabled, ansi::white);
            std::cout << std::string(6, ' ') << will_lines[index] << '\n';
        }
        for (auto row = end - start; row < will_rows; ++row) std::cout << '\n';
        paint(ansi_enabled, ansi::boot_blue);
        centered_line("LAST WILL PAGE " + std::to_string(page + 1) + '/' +
            std::to_string(will_pages));
    } else {
        const auto terminal = terminal_size();
        const auto image_rows = std::clamp<std::size_t>(
            terminal.rows > 13 ? terminal.rows - 13 : 12, 12, 24);
        const auto image_columns = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::lround(image_rows * 1.68)), 24, 44);
        const auto left_padding = panel_width > image_columns
            ? (panel_width - image_columns) / 2 : 0;
        const auto& lines = sentinel_mascot_lines(
            root, ansi_enabled, image_columns, image_rows);
        for (std::size_t row = 0; row < image_rows; ++row) {
            std::cout << std::string(left_padding, ' ') << lines[row] << '\n';
        }
        paint(ansi_enabled, ansi::boot_gold);
        centered_line("[ WAYKEEPER ID // " + ui_settings().room_code + " ]");
        paint(ansi_enabled, ansi::boot_blue);
        if (mode == SentinelMode::Quiet) {
            centered_line("QUIET WATCH // FIELD UNIT AT REST");
            centered_line("PRIVATE ARCHIVE SEALED");
            centered_line("NO RESPONSE REQUIRED");
        } else {
            const auto& active = cards[active_card % cards.size()];
            for (const auto line : active) centered_line(line);
        }
    }

    paint(ansi_enabled, ansi::muted);
    std::ostringstream footer;
    footer << "PUBLIC MODE // DATA SEALED // ANSI MOVIE " << movie_speed_label();
    if (mode != SentinelMode::Blackout) {
        footer << " // CARD " << active_card + 1 << '/' << public_cards;
        if (show_last_will) footer << " // OPT-IN FOUND-DEVICE MESSAGE";
    }
    centered_line(footer.str());
    paint(ansi_enabled, mode == SentinelMode::Blackout ? ansi::muted : ansi::boot_blue);
    std::cout << '+' << std::string(panel_width - 2, '=') << "+\n" << std::flush;
    paint(ansi_enabled, ansi::reset);
}

void sentinel_idle_screen(
    const std::filesystem::path& root, const SentinelMode mode,
    const bool ansi_enabled) {
    auto& deadman = deadman_controller();
    deadman.disarm();
    SentinelEscapeSequence wake;
    std::size_t card = 0;
    render_sentinel_idle_frame(root, mode, card, ansi_enabled);
    for (;;) {
        const auto duration = mode != SentinelMode::Blackout
            ? sentinel_card_duration(ui_settings().sentinel_movie_speed)
            : std::chrono::milliseconds{1000};
        const auto key = read_terminal_key_for(duration);
        if (key.status == TerminalKeyStatus::Timeout) {
            if (mode != SentinelMode::Blackout) {
                ++card;
                render_sentinel_idle_frame(root, mode, card, ansi_enabled);
            }
            continue;
        }
        if (key.status == TerminalKeyStatus::Unavailable ||
            key.status == TerminalKeyStatus::EndOfInput) break;
        if (wake.push(key.key)) break;
    }
    deadman.arm();
    set_title(ansi_enabled);
    clear_launch_window(ansi_enabled);
}

std::string mascot_mode(const std::string_view incident) {
    if (incident.find("Nuclear") != std::string_view::npos ||
        incident.find("Radiological") != std::string_view::npos) {
        return "NUCLEAR / VAULT-BLUE MODE";
    }
    if (incident.find("Zombie") != std::string_view::npos) return "ZOMBIE / FALLOUT MODE";
    return "GENERAL SURVIVAL MODE";
}

void mascot_screen(const OperatorProfile& profile, const bool ansi_enabled) {
    clear(ansi_enabled);
    paint(ansi_enabled, ansi::amber_bar);
    std::cout << " WAYKEEPER // INCIDENT MODE LOCKED ";
    paint(ansi_enabled, ansi::reset);
    std::cout << "  " << mascot_mode(profile.incident) << '\n';
    rule('=');
    const auto asset = mascot_asset(resource_root(), profile.incident);
    if (!asset.empty() && image_support_available()) {
        const auto terminal = terminal_size();
        const auto image_columns = std::max<std::size_t>(8, std::min<std::size_t>(35,
            terminal.columns > 2 ? terminal.columns - 2 : terminal.columns));
        const auto image_rows = std::max<std::size_t>(4, std::min<std::size_t>(21,
            terminal.rows > 9 ? terminal.rows - 9 : 4));
        ImageInfo image_info;
        std::string image_error;
        if (!render_image_ansi(
                asset, image_columns, image_rows, ansi_enabled,
                std::cout, image_info, image_error)) {
            paint(ansi_enabled, ansi::red);
            std::cout << "MASCOT RENDER UNAVAILABLE // " << image_error << '\n';
        }
    } else {
        paint(ansi_enabled, ansi::muted);
        std::cout << "MASCOT IMAGE UNAVAILABLE // " << mascot_mode(profile.incident) << '\n';
    }
    paint(ansi_enabled, ansi::cyan);
    std::cout << "MODE      " << mascot_mode(profile.incident) << '\n'
              << "ROOM      " << ui_settings().room_code << " // MASCOT ID BADGE\n"
              << "INCIDENT  " << upper(profile.incident) << '\n'
              << "TERRAIN   " << upper(profile.terrain) << '\n';
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
}

std::filesystem::path boot_unlock_asset(
    const std::filesystem::path& root, const BootUnlock unlock) {
    const std::string filename = unlock == BootUnlock::VaultBlueRaider
        ? "Vault-Tec-WayKeeper-Easteregg.png"
        : "Raider-Waykeeper.png";
    for (const auto& directory : {
             root / "RES" / "WayKeeper TM", root.parent_path() / "RES" / "WayKeeper TM"}) {
        const auto candidate = directory / filename;
        if (std::filesystem::exists(candidate)) return candidate;
    }
    return {};
}

void boot_unlock_splash(
    const BootUnlock unlock, const std::filesystem::path& root,
    const bool ansi_enabled, const std::string_view state_warning = {}) {
    if (unlock == BootUnlock::None) return;
    clear(ansi_enabled);
    paint(ansi_enabled, unlock == BootUnlock::VaultBlueRaider
        ? ansi::cyan_bar : ansi::amber_bar);
    std::cout << " WAYKEEPER // "
              << (unlock == BootUnlock::VaultBlueRaider ? "RADIOACTIVE" : "RAIDER") << ' ';
    paint(ansi_enabled, ansi::reset);
    std::cout << "  " << (unlock == BootUnlock::VaultBlueRaider
        ? "VAULT-BLUE RAIDER" : "RAIDER") << " SPLASH\n";
    rule('=');
    const auto asset = boot_unlock_asset(root, unlock);
    if (!asset.empty() && image_support_available()) {
        const auto terminal = terminal_size();
        const auto image_columns = std::max<std::size_t>(8, std::min<std::size_t>(35,
            terminal.columns > 2 ? terminal.columns - 2 : terminal.columns));
        const auto image_rows = std::max<std::size_t>(4, std::min<std::size_t>(21,
            terminal.rows > 9 ? terminal.rows - 9 : 4));
        ImageInfo image_info;
        std::string image_error;
        if (!render_image_ansi(
                asset, image_columns, image_rows, ansi_enabled,
                std::cout, image_info, image_error)) {
            paint(ansi_enabled, ansi::red);
            std::cout << "EASTER EGG IMAGE UNAVAILABLE // " << image_error << '\n';
        }
    }
    paint(ansi_enabled, ansi::cyan);
    std::cout << "ACHIEVEMENT  " << (unlock == BootUnlock::VaultBlueRaider
        ? "COOKBOOK ARCHIVE READER" : "MOONSHINER ARCHIVE READER") << '\n'
              << "STATE        ONE-SHOT SPLASH CONSUMED\n";
    if (!state_warning.empty()) {
        paint(ansi_enabled, ansi::red);
        std::cout << "STATE WARNING " << state_warning << '\n';
    }
    paint(ansi_enabled, ansi::reset);
    (void)pager_prompt("> PRESS ENTER TO CONTINUE_", ansi_enabled, true);
}

void shell_header(
    const OperatorProfile& profile,
    const bool ollama_ready,
    const std::size_t documents,
    const bool ansi_enabled,
    std::string_view section) {
    clear(ansi_enabled);
    if (compact_display()) {
        const std::string title = " WAYKEEPER // " + std::string(section) + " ";
        paint(ansi_enabled, ansi::amber_bar);
        std::cout << title;
        paint(ansi_enabled, ansi::white);
        std::cout << "  " << timestamp();
        paint(ansi_enabled, ansi::reset);
        finish_aligned_line(title.size() + timestamp().size() + 2);
        paint(ansi_enabled, ansi::cyan);
        expanded_line(
            "OP " + upper(profile.name) + " | ROOM " + ui_settings().room_code +
            " | " + battery_rail(host_battery()) + " | DOC " +
            std::to_string(documents) + " | GUIDE " +
            std::string(ollama_ready ? "LOCAL" : "OFF") + " | ESC BACK");
        paint(ansi_enabled, ansi::reset);
        rule('-');
        return;
    }
    if (ui_settings().layout_mode == "minimal") {
        const std::string minimal_title = " WAYKEEPER ";
        const std::string minimal_context = "  " + timestamp() + "  //  MIN  //  " +
            std::string(section) + "  //  " + ui_settings().room_code;
        paint(ansi_enabled, ansi::amber_bar);
        std::cout << minimal_title;
        paint(ansi_enabled, ansi::white);
        std::cout << minimal_context;
        paint(ansi_enabled, ansi::reset);
        finish_aligned_line(minimal_title.size() + minimal_context.size());
        rule('=');
        paint(ansi_enabled, ansi::amber);
        expanded_line(
            "OP " + upper(profile.name) + "  |  " + upper(profile.incident) + "  |  " +
            upper(profile.terrain) + "  |  " + battery_rail(host_battery()));
        paint(ansi_enabled, ansi::muted);
        expanded_line(
            "DOC " + std::to_string(documents) + "  |  CARD " +
            std::to_string(reviewed_cards().size()) + "  |  GUIDE " +
            std::string(ollama_ready ? "LOCAL" : "OFF") + "  |  " +
            (profile.simulation() ? "SIM" : "ACTIVE"));
        paint(ansi_enabled, ansi::green);
        expanded_line(
            "F1 CARD | F2 DOC | F3 GUIDE | F5 SYS | F6 MAP | F7 HERB | SCHEM | "
            "F8 LOG | F10 INV | Q");
        paint(ansi_enabled, ansi::reset);
        rule('-');
        return;
    }
    const std::string header_title = " WAYKEEPER // OFF-GRID OPERATIONS TERMINAL ";
    const std::string header_context = "  " + timestamp() + "  //  " + std::string(section);
    paint(ansi_enabled, ansi::amber_bar);
    std::cout << header_title;
    paint(ansi_enabled, ansi::reset);
    paint(ansi_enabled, ansi::white);
    std::cout << header_context;
    finish_aligned_line(header_title.size() + header_context.size());
    paint(ansi_enabled, ansi::reset);
    rule('=');

    const std::string operator_name = upper(profile.name);
    const std::string incident = compact_slashes(upper(profile.incident));
    const std::string terrain = compact_slashes(upper(profile.terrain));
    const std::string room = "ROOM " + ui_settings().room_code;
    const std::string theme = "THEME " + upper(ui_settings().theme);
    const std::string battery = battery_rail(host_battery());
    const std::string library = "LIB " + std::to_string(documents);
    const std::string cards = "CARD " + std::to_string(reviewed_cards().size());
    const std::string guide = std::string("GUIDE ") + (ollama_ready ? "LOCAL" : "OFF");
    const std::string context = profile.simulation() ? "SIM" : "ACTIVE";
    const std::size_t status_width =
        3 + operator_name.size() + 12 + incident.size() + 11 + terrain.size() +
        3 + room.size() + 3 + theme.size() + 3 + battery.size() + 3 + library.size() +
        3 + cards.size() + 3 + guide.size() + 3 + context.size();
    paint(ansi_enabled, ansi::amber);
    std::cout << "OP ";
    paint(ansi_enabled, ansi::white);
    std::cout << operator_name << " | ";
    paint(ansi_enabled, ansi::amber);
    std::cout << "INCIDENT ";
    paint(ansi_enabled, profile.simulation() ? ansi::cyan : ansi::red);
    std::cout << incident << " | ";
    paint(ansi_enabled, ansi::amber);
    std::cout << "TERRAIN ";
    paint(ansi_enabled, ansi::white);
    std::cout << terrain << " | ";
    paint(ansi_enabled, ansi::muted);
    std::cout << room << " | " << theme << " | ";
    paint(ansi_enabled, ansi::green);
    std::cout << battery << " | ";
    paint(ansi_enabled, ansi::muted);
    std::cout << library << " | " << cards << " | ";
    paint(ansi_enabled, ollama_ready ? ansi::green : ansi::red);
    std::cout << guide << " | ";
    paint(ansi_enabled, profile.simulation() ? ansi::cyan : ansi::red);
    std::cout << context;
    paint(ansi_enabled, ansi::reset);
    finish_aligned_line(status_width);
    rule('-');
    paint(ansi_enabled, ansi::amber);
    expanded_line(
        "F1 CARDS | F2 LIBRARY | F3 GUIDE | F4 PROFILE | F5 SYSTEM | F6 MAP | SCHEMATICS | "
        "F7 HERBS | F8 LOG | F9 SOCIETY | F10 INVENTORY | F11 OOBE | F12 ABOUT | ESC BACK/EXIT");
    paint(ansi_enabled, ansi::reset);
    rule('-');
}

void map_view_header(
    const OperatorProfile& profile, const bool ansi_enabled,
    const std::string_view map_name) {
    clear(ansi_enabled);
    paint(ansi_enabled, ansi::amber_bar);
    const std::string title = " MAP // " + upper(std::string(map_name)) + " ";
    std::cout << title;
    paint(ansi_enabled, ansi::white);
    const std::string context = "  " + ui_settings().room_code + " | " + upper(profile.name);
    std::cout << context;
    paint(ansi_enabled, ansi::reset);
    finish_aligned_line(title.size() + context.size());
}

std::vector<std::filesystem::path> credits_music_files(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    for (const auto& directory : {root / "RES" / "Music", root.parent_path() / "RES" / "Music"}) {
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
            if (!error && entry.is_regular_file() && entry.path().extension() == ".mp4") {
                files.push_back(entry.path());
            }
        }
        if (!files.empty()) break;
    }
    std::sort(files.begin(), files.end());
    return files;
}

class CreditsMusicLoop {
public:
    explicit CreditsMusicLoop(const std::filesystem::path& root) : files_(credits_music_files(root)) {
#ifdef __APPLE__
        if (files_.empty() || !std::filesystem::exists("/usr/bin/afplay")) return;
        pid_ = fork();
        if (pid_ == 0) {
            setpgid(0, 0);
            std::string command = "while :; do ";
            for (const auto& file : files_) {
                std::string escaped = file.string();
                std::size_t position = 0;
                while ((position = escaped.find('\'', position)) != std::string::npos) {
                    escaped.replace(position, 1, "'\\''");
                    position += 4;
                }
                command += "/usr/bin/afplay '" + escaped + "' || exit; ";
            }
            command += "done";
            execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        if (pid_ > 0) setpgid(pid_, pid_);
#endif
    }
    ~CreditsMusicLoop() {
#ifdef __APPLE__
        if (pid_ > 0) {
            kill(-pid_, SIGTERM);
            waitpid(pid_, nullptr, 0);
        }
#endif
    }
    bool active() const {
#ifdef __APPLE__
        return pid_ > 0;
#else
        return false;
#endif
    }
    std::size_t track_count() const { return files_.size(); }

private:
    std::vector<std::filesystem::path> files_;
#ifdef __APPLE__
    pid_t pid_{-1};
#endif
};

void vim_text_screen(
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled,
    std::string_view section, std::string_view heading, std::string_view text,
    const std::filesystem::path& icon = {}, std::string_view quote = {}) {
    std::size_t offset = 0;
    for (;;) {
        auto geometry = pager_geometry();
        const bool show_icon = !icon.empty() && std::filesystem::exists(icon) && image_support_available();
        constexpr std::size_t icon_columns = 24;
        constexpr std::size_t icon_rows = 12;
        if (show_icon) geometry.rows = geometry.rows > icon_rows + 1
            ? geometry.rows - icon_rows - 1 : geometry.rows;
        if (!quote.empty()) geometry.rows = geometry.rows > 3 ? geometry.rows - 3 : geometry.rows;
        const auto lines = pager_lines(text, geometry.columns);
        shell_header(profile, ollama_ready, documents, ansi_enabled, section);
        paint(ansi_enabled, ansi::amber);
        std::cout << heading << '\n';
        paint(ansi_enabled, ansi::reset);
        if (!quote.empty()) {
            paint(ansi_enabled, ansi::small_green);
            std::istringstream quote_lines{std::string(quote)};
            std::string quote_line;
            std::cout << '\n';
            while (std::getline(quote_lines, quote_line)) centered_line(quote_line);
            paint(ansi_enabled, ansi::reset);
        }
        if (show_icon) {
            ImageInfo image_info;
            std::string image_error;
            render_image_ansi(
                icon, icon_columns, icon_rows, ansi_enabled,
                std::cout, image_info, image_error);
            paint(ansi_enabled, ansi::cyan);
            const auto color_mode = terminal_color_mode(ansi_enabled);
            std::cout << "ALBUM ART // "
                      << (color_mode == TerminalColorMode::TrueColor
                              ? "RGB24 / 16.7M COLORS"
                              : color_mode == TerminalColorMode::Ansi256
                                  ? "ANSI-256" : "MONOCHROME")
                      << " // SOURCE " << image_info.source_width << 'x'
                      << image_info.source_height << '\n';
            paint(ansi_enabled, ansi::reset);
        }
        rule();
        render_vim_text(lines, offset, geometry, ansi_enabled);
        const auto command = pager_prompt("VIEW> ", ansi_enabled, true);
        if (command == "q" || command == "back" || command == "escape") return;
        if (pager_scroll(command, offset, lines, geometry.rows)) continue;
        const auto maximum = pager_max_offset(lines, geometry.rows);
        if (command == " " || command == "space" || command == "pgdn") {
            offset = std::min(maximum, offset + geometry.rows);
        } else if (command == "b" || command == "pgup") {
            offset = offset > geometry.rows ? offset - geometry.rows : 0;
        }
    }
}

template <typename Options>
std::string choose_option(
    std::string_view heading, const Options& options, const bool ansi_enabled) {
    VimMenuState state;
    std::vector<std::string> items(options.begin(), options.end());
    for (;;) {
        clear(ansi_enabled);
        paint(ansi_enabled, ansi::amber_bar);
        std::cout << " OFF-GRID PROFILE CONFIGURATION ";
        paint(ansi_enabled, ansi::reset);
        std::cout << "  //  INPUT LOCK\n";
        rule('=');
        paint(ansi_enabled, ansi::amber);
        std::cout << heading << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows(9);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER SELECT\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("SELECT> ", ansi_enabled, true);
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            return options[*choice];
        }
    }
}

OperatorProfile onboarding(const bool ansi_enabled, const OperatorProfile* previous = nullptr) {
    logo(ansi_enabled);
    paint(ansi_enabled, ansi::bold);
    std::cout << (previous ? "PROFILE RECONFIGURATION" : "WELCOME // FIRST-RUN CONFIGURATION") << '\n';
    paint(ansi_enabled, ansi::reset);
    std::cout << "This local context controls the terminal banner and future relevance filters.\n"
                 "It is not a diagnosis, official incident declaration, or location tracker.\n\n";

    OperatorProfile profile;
    profile.name = prompt("OPERATOR NAME> ", ansi_enabled);
    if (profile.name.empty()) profile.name = previous ? previous->name : "Operator";
    profile.incident = choose_option("WHAT KIND OF DISASTER IS IT?", incident_options(), ansi_enabled);
    profile.terrain = choose_option("WHERE ARE YOU / WHAT IS THE TERRAIN?", terrain_options(), ansi_enabled);

    paint(ansi_enabled, ansi::cyan);
    std::cout << "\nPROFILE SUMMARY\n";
    paint(ansi_enabled, ansi::reset);
    std::cout << "  NAME      " << profile.name << '\n'
              << "  INCIDENT  " << profile.incident << '\n'
              << "  TERRAIN   " << profile.terrain << '\n';
    std::string error;
    if (!save_profile(profile, error)) {
        paint(ansi_enabled, ansi::red);
        std::cout << "PROFILE NOT SAVED: " << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
    }
    mascot_screen(profile, ansi_enabled);
    return profile;
}

void show_card(
    const GuideCard& card, const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    std::ostringstream content;
    content << card.answer << "\n\nSOURCE   " << card.source_url
            << "\nREVIEWED " << card.reviewed_on;
    vim_text_screen(
        profile, ollama_ready, documents, ansi_enabled,
        "SURVIVAL CARD", "CARD> " + upper(card.title), content.str());
}

void show_candidate_card(
    const CandidateCard& card, const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    std::ostringstream content;
    content << "STATUS   CANDIDATE // NOT REVIEWED // NOT LIVE"
            << "\nRISK     " << upper(card.risk)
            << "\nTRUST    " << upper(card.trust)
            << "\nID       " << card.id
            << "\n\n" << card.answer
            << "\n\nLIMITS\n" << card.limits
            << "\n\nSOURCE\n"
            << "DOC      " << card.source_document
            << "\nPDF      " << card.source_pdf
            << "\nPAGE     " << card.source_pages
            << "\nPUBLISHED " << card.source_published
            << "\nCHECKED  " << card.cross_checked
            << "\nCURRENT  " << card.cross_check_url
            << "\n\n" << card.source_note
            << "\n\nREVIEW GATE\n"
               "A named human reviewer must ACCEPT, REVISE, MERGE, or REJECT this card.\n"
               "Viewing this page does not promote it into the trusted Guide.";
    vim_text_screen(
        profile, ollama_ready, documents, ansi_enabled,
        "F1 / PHASE 1 REVIEW // CANDIDATE", "REVIEW> " + upper(card.title), content.str());
}

void candidate_cards_screen(
    const std::vector<CandidateCard>& cards, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents, const bool ansi_enabled) {
    std::vector<std::string> items;
    items.reserve(cards.size());
    for (const auto& card : cards) {
        items.push_back("[" + upper(card.risk) + "] " + card.title);
    }
    VimMenuState state;
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F1 / PHASE 1 REVIEW QUEUE");
        paint(ansi_enabled, ansi::red);
        std::cout << "CANDIDATES ONLY // NOT REVIEWED // NOT USED BY GUIDE\n";
        paint(ansi_enabled, ansi::reset);
        const auto rows = vim_menu_rows(14);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O INSPECT  |  Q BACK  |  :OPEN <N>\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("REVIEW> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            show_candidate_card(
                cards[*choice], profile, ollama_ready, documents, ansi_enabled);
        }
    }
}

void cards_screen(
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    const auto& live_cards = reviewed_cards();
    std::string candidate_error;
    const auto candidates = load_candidate_cards(
        resource_root() / "cards" / "candidates" / "phase-1", candidate_error);
    const std::vector<std::string> collections{
        "REVIEWED / LIVE  // " + std::to_string(live_cards.size()) + " TRUSTED CARDS",
        "PHASE 1 / REVIEW QUEUE  // " +
            (candidate_error.empty() ? std::to_string(candidates.size()) + " CANDIDATES"
                                     : std::string("UNAVAILABLE"))};
    VimMenuState state;
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F1 / CARDS + REVIEW");
        paint(ansi_enabled, ansi::green);
        std::cout << "TRUSTED CARDS AND CANDIDATES REMAIN STRICTLY SEPARATED\n";
        paint(ansi_enabled, ansi::reset);
        const auto rows = vim_menu_rows(14);
        render_vim_menu(collections, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN COLLECTION  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("CARDS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, collections.size(), rows)) continue;
        const auto collection = vim_menu_choice(command, state, collections.size());
        if (!collection) continue;
        if (*collection == 0) {
            std::vector<std::string> items;
            items.reserve(live_cards.size());
            for (const auto& card : live_cards) items.push_back(card.title);
            VimMenuState live_state;
            for (;;) {
                shell_header(profile, ollama_ready, documents, ansi_enabled, "F1 / REVIEWED CARDS");
                const auto live_rows = vim_menu_rows();
                render_vim_menu(items, live_state, live_rows, ansi_enabled);
                paint(ansi_enabled, ansi::muted);
                std::cout << "ACTION  ENTER/O OPEN  |  Q BACK  |  :OPEN <N>\n";
                paint(ansi_enabled, ansi::reset);
                const auto live_command = pager_prompt("LIVE CARDS> ", ansi_enabled);
                if (live_command == "q" || live_command == "back" || live_command == "escape") {
                    break;
                }
                if (navigate_vim_menu(live_command, live_state, items.size(), live_rows)) continue;
                if (const auto choice = vim_menu_choice(live_command, live_state, items.size())) {
                    show_card(
                        live_cards[*choice], profile, ollama_ready, documents, ansi_enabled);
                }
            }
        } else if (!candidate_error.empty()) {
            vim_text_screen(
                profile, ollama_ready, documents, ansi_enabled,
                "F1 / PHASE 1 REVIEW", "REVIEW QUEUE UNAVAILABLE", candidate_error);
        } else {
            candidate_cards_screen(
                candidates, profile, ollama_ready, documents, ansi_enabled);
        }
    }
}

void pdf_image_viewer(
    const std::filesystem::path& pdf_path, const std::string& document_id,
    std::string_view title, const std::size_t page, const std::size_t page_count,
    const std::filesystem::path& root, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents, const bool ansi_enabled,
    std::string_view safety_note = {}) {
    ImageViewport viewport;
    std::filesystem::path image_path;
    std::string error;
    if (!ensure_pdf_page_image(
            pdf_path, root / "tmp" / "pdf-page-images", document_id, page,
            image_path, error)) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "ANSI PDF IMAGE");
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    for (;;) {
        const auto terminal = terminal_size();
        const auto image_columns = std::clamp<std::size_t>(
            terminal.columns > 4 ? terminal.columns - 4 : terminal.columns, 40, 96);
        const auto image_rows = std::clamp<std::size_t>(
            terminal.rows > 17 ? terminal.rows - 17 : 8, 8, 28);
        std::ostringstream rendered;
        ImageInfo info;
        error.clear();
        if (!render_image_ansi(
                image_path, image_columns, image_rows, ansi_enabled,
                rendered, info, error, viewport)) {
            shell_header(profile, ollama_ready, documents, ansi_enabled, "ANSI PDF IMAGE");
            paint(ansi_enabled, ansi::red);
            std::cout << error << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
            return;
        }

        shell_header(profile, ollama_ready, documents, ansi_enabled, "ANSI PDF IMAGE");
        if (!safety_note.empty()) {
            paint(ansi_enabled, ansi::red);
            std::cout << safety_note << '\n';
        }
        paint(ansi_enabled, ansi::cyan);
        std::cout << upper(std::string(title)) << '\n';
        paint(ansi_enabled, ansi::amber);
        std::cout << "PDF PAGE " << page << " / " << page_count << "  |  SOURCE "
                  << info.source_width << 'X' << info.source_height << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        std::cout << rendered.str();
        rule();
        paint(ansi_enabled, ansi::cyan);
        std::cout << "IMAGE VIEW  " << std::fixed << std::setprecision(1) << viewport.zoom
                  << "X  |  CACHED OFFLINE\n";
        paint(ansi_enabled, ansi::muted);
        std::cout << "PAN ARROWS/HJKL  |  +/- ZOOM  |  R RESET  |  O OPENPDF  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("IMAGE> ", ansi_enabled);
        const auto half_window = 0.5 / viewport.zoom;
        const auto pan_step = 0.2 / viewport.zoom;
        if (command == "left" || command == "h") {
            viewport.center_x = std::max(half_window, viewport.center_x - pan_step);
        } else if (command == "right" || command == "l") {
            viewport.center_x = std::min(1.0 - half_window, viewport.center_x + pan_step);
        } else if (command == "up" || command == "k") {
            viewport.center_y = std::max(half_window, viewport.center_y - pan_step);
        } else if (command == "down" || command == "j") {
            viewport.center_y = std::min(1.0 - half_window, viewport.center_y + pan_step);
        } else if (command == "+" || command == "=") {
            viewport.zoom = std::min(8.0, viewport.zoom * 2.0);
        } else if (command == "-" || command == "_") {
            viewport.zoom = std::max(1.0, viewport.zoom / 2.0);
            const auto new_half_window = 0.5 / viewport.zoom;
            viewport.center_x = std::clamp(viewport.center_x, new_half_window, 1.0 - new_half_window);
            viewport.center_y = std::clamp(viewport.center_y, new_half_window, 1.0 - new_half_window);
        } else if (command == "r" || command == "reset") {
            viewport = {};
        } else if (command == "openpdf" || command == "o") {
#ifdef __APPLE__
            const std::string shell_command = "open \"" + pdf_path.string() + "\"";
            std::system(shell_command.c_str());
#endif
        } else if (command == "q" || command == "back" || command == "escape") {
            return;
        }
    }
}

bool acknowledge_restricted_cookbook(const bool ansi_enabled) {
    paint(ansi_enabled, ansi::red);
    std::cout << "AGE RESTRICTED // 18+ ONLY\n"
                 "ILLEGAL CONTENT - FOR REFERENCE PURPOSES ONLY - HISTORIC\n"
                 "RESTRICTED UNDERGROUND ARCHIVE // UNVERIFIED AND NOT SURVIVAL GUIDANCE\n"
                 "INCLUDES ILLEGAL, VIOLENT, AND POTENTIALLY LETHAL MATERIAL.\n"
                 "LOCAL RESTRICTED INDEX ONLY // EXCLUDED FROM GUIDE + DEEPSEARCH\n";
    paint(ansi_enabled, ansi::amber);
    const auto confirmation = lower(prompt(
        "TYPE I AM 18 AND ACKNOWLEDGE TO CONTINUE> ", ansi_enabled));
    paint(ansi_enabled, ansi::reset);
    return confirmation == "i am 18 and acknowledge";
}

void reader(
    SurvivalLibrary& library, std::size_t document_index, std::size_t page,
    const OperatorProfile& profile, const bool ollama_ready, const bool ansi_enabled,
    bool restricted_access_granted = false) {
    const auto& document = library.documents()[document_index];
    const bool pdf_source = document.pdf_path.extension() == ".pdf";
    const bool restricted_cookbook =
        document.category.starts_with("Cookbook-Underground-Restricted");
    if (restricted_cookbook && !restricted_access_granted) {
        if (!acknowledge_restricted_cookbook(ansi_enabled)) return;
    }
    BootUnlock reader_unlock = BootUnlock::None;
    if (document.id == "textfiles-home-alcohol" ||
        document.category.find("Distilling") != std::string::npos ||
        document.title.find("Moonshin") != std::string::npos) {
        reader_unlock = BootUnlock::Raider;
    }
    if (restricted_cookbook || document.title.find("Cookbook") != std::string::npos) {
        reader_unlock = BootUnlock::VaultBlueRaider;
    }
    std::string unlock_error;
    const bool unlock_scheduled = reader_unlock != BootUnlock::None &&
        schedule_boot_unlock(boot_unlock_path(), reader_unlock, unlock_error);
    bool show_unlock_notice = unlock_scheduled || !unlock_error.empty();
    page = std::max<std::size_t>(1, std::min(page, document.pages));
    std::size_t line_offset = 0;
    bool previous_page_bottom = false;
    for (;;) {
        const auto geometry = pager_geometry();
        const auto text = library.read_page(document_index, page);
        const auto lines = pager_lines(text ? *text : std::string_view{}, geometry.columns);
        if (previous_page_bottom) {
            line_offset = pager_max_offset(lines, geometry.rows);
            previous_page_bottom = false;
        }
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            pdf_source ? "PDF SOURCE READER" : "TEXT SOURCE READER");
        paint(ansi_enabled, ansi::cyan);
        std::cout << upper(document.title) << '\n';
        if (document.category.find("Regulations") != std::string::npos) {
            paint(ansi_enabled, ansi::red);
            std::cout << "LAW CHANGES // VERIFY CURRENT SEASON, LICENSES, LIMITS, AND LOCAL RULES\n";
        }
        if (document.category.find("Wildlife-Safety") != std::string::npos ||
            document.category.find("Wolf-Safety") != std::string::npos) {
            paint(ansi_enabled, ansi::red);
            std::cout << "WILDLIFE SAFETY // AVOIDANCE + SPECIES-SPECIFIC GUIDANCE COME FIRST\n";
        }
        if (document.category.find("Shelter-Improvised") != std::string::npos) {
            paint(ansi_enabled, ansi::red);
            std::cout << "SITE HAZARDS // CHECK DEADFALL, FLOOD, AVALANCHE, FIRE, VENTILATION + EXPOSURE\n";
        }
        if (document.category.find("Shelter-Cabin") != std::string::npos ||
            document.category.find("Shelter-Cold-Climate") != std::string::npos) {
            paint(ansi_enabled, ansi::red);
            std::cout << "PERMANENT STRUCTURE // CURRENT CODE, PERMITS, ENGINEERING, FIRE + SNOW LOADS REQUIRED\n";
        }
        if (document.category.starts_with("Religion-")) {
            paint(ansi_enabled, ansi::cyan);
            std::cout << "BELIEF + CULTURAL REFERENCE // SOURCE IDENTITY PRESERVED; NO ENDORSEMENT IMPLIED\n";
        }
        if (document.category.starts_with("Civics-")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "CIVICS REFERENCE // VERIFY CURRENT LAW, OFFICES, AUTHORITIES, AND EMERGENCY POWERS\n";
        }
        if (document.category.starts_with("FOIA-")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "HISTORICAL ABUSE RECORD // NOT OPERATIONAL GUIDANCE; COERCION IS HARMFUL\n";
        }
        if (document.category.starts_with("Philosophy-")) {
            paint(ansi_enabled, ansi::cyan);
            std::cout << "PHILOSOPHY READER // HISTORICAL SOURCE OR RIGHTS-GATED BIBLIOGRAPHIC RECORD\n";
        }
        if (document.category.starts_with("Economy-")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "TRADE REFERENCE // FAIR MEASURES, CONSENT, RECORDS + CURRENT LAW STILL APPLY\n";
        }
        if (document.category.starts_with("Rebuilding-")) {
            paint(ansi_enabled, ansi::cyan);
            std::cout << "RECOVERY DOCTRINE // ADAPT TO LOCAL NEEDS, AUTHORITY, RESOURCES + CONSENT\n";
        }
        if (document.category.starts_with("Electronics-")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "ELECTRICAL HAZARD // DE-ENERGIZE, LOCK OUT, VERIFY ZERO ENERGY + DISCHARGE STORAGE\n";
        }
        if (document.category.starts_with("Automotive-")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "VEHICLE HAZARD // CHOCK + RATED SUPPORTS + MODEL-SPECIFIC PROCEDURES REQUIRED\n";
        }
        if (document.category.starts_with("Product-Manual-Archives")) {
            paint(ansi_enabled, ansi::cyan);
            std::cout << "CATALOG INTELLIGENCE // RIGHTS, REVISIONS + OFFLINE AVAILABILITY VARY BY SOURCE\n";
        }
        if (document.category.starts_with("Agriculture-")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "FIELD REFERENCE // LOCAL CONDITIONS + CURRENT LABELS, LAW, EXTENSION + VETERINARY GUIDANCE CONTROL\n";
        }
        if (document.category.starts_with("Underground-TEXTFILES")) {
            paint(ansi_enabled, ansi::red);
            std::cout << "TEXTFILES SOURCE // UNDERGROUND, HISTORICAL, UNVERIFIED + RIGHTS UNCLEARED\n";
            if (document.category.find("Distilling") != std::string::npos) {
                std::cout << "DISTILLING/SALE REGULATED // FIRE, PRESSURE + TOXIC FRACTION HAZARDS\n";
            }
        }
        if (restricted_cookbook) {
            paint(ansi_enabled, ansi::red);
            std::cout << "COOKBOOK // AGE RESTRICTED 18+ / RIGHTS UNCLEARED\n"
                         "ILLEGAL CONTENT - FOR REFERENCE PURPOSES ONLY - HISTORIC\n"
                         "LOCAL INDEX ONLY // NOT GUIDE-INDEXED // NOT VERIFIED OR ENDORSED\n";
        }
        if (show_unlock_notice) {
            paint(ansi_enabled, unlock_scheduled ? ansi::green : ansi::red);
            if (unlock_scheduled) {
                std::cout << "EASTER EGG ARMED // "
                          << (reader_unlock == BootUnlock::VaultBlueRaider
                              ? "VAULT-BLUE RAIDER" : "RAIDER")
                          << " SPLASH WILL LOAD ON NEXT BOOT\n";
            } else {
                std::cout << "EASTER EGG STATE ERROR // " << unlock_error << '\n';
            }
            show_unlock_notice = false;
        }
        paint(ansi_enabled, ansi::amber);
        std::cout << (pdf_source ? "PDF PAGE " : "READER PAGE ")
                  << page << " / " << document.pages << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        render_pager(lines, line_offset, geometry, ansi_enabled);
        const auto command = pager_prompt("READER> ", ansi_enabled);
        const auto maximum = pager_max_offset(lines, geometry.rows);
        if (pager_scroll(command, line_offset, lines, geometry.rows)) continue;
        if (command == " " || command == "space" || command == "pgdn") {
            if (line_offset < maximum) line_offset = std::min(maximum, line_offset + geometry.rows);
            else if (page < document.pages) { ++page; line_offset = 0; }
        } else if (command == "b" || command == "pgup") {
            if (line_offset > 0) {
                line_offset = line_offset > geometry.rows ? line_offset - geometry.rows : 0;
            } else if (page > 1) {
                --page;
                previous_page_bottom = true;
            }
        } else if ((command == "next" || command == "n") && page < document.pages) {
            ++page;
            line_offset = 0;
        } else if ((command == "prev" || command == "p") && page > 1) {
            --page;
            line_offset = 0;
        }
        else if (command.rfind("goto ", 0) == 0) {
            try {
                page = std::max<std::size_t>(1, std::min(
                    static_cast<std::size_t>(std::stoull(command.substr(5))), document.pages));
                line_offset = 0;
            } catch (...) {}
        } else if (command == "image" || command == "i") {
            if (pdf_source) {
                pdf_image_viewer(
                    document.pdf_path, document.id, document.title, page, document.pages,
                    resource_root(), profile, ollama_ready, library.documents().size(), ansi_enabled,
                    restricted_cookbook
                        ? "RESTRICTED SOURCE // UNVERIFIED, ILLEGAL, VIOLENT + POTENTIALLY LETHAL CONTENT"
                        : "");
            } else {
                paint(ansi_enabled, ansi::red);
                std::cout << "NO PAGE IMAGE // THIS EDITION IS A PLAIN-TEXT READER SOURCE\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            }
        } else if (command == "openpdf" || command == "open" || command == "o") {
#ifdef __APPLE__
            const std::string shell_command = "open \"" + document.pdf_path.string() + "\"";
            std::system(shell_command.c_str());
#endif
        } else if (command == "back" || command == "q" || command == "escape") return;
    }
}

bool society_category(const std::string_view category) {
    return category.starts_with("Religion-") || category.starts_with("Civics-") ||
        category.starts_with("FOIA-") || category.starts_with("Philosophy-") ||
        category.starts_with("Economy-") || category.starts_with("Rebuilding-") ||
        category.starts_with("Underground-TEXTFILES");
}

void search_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled, std::string query = {}, const bool society_only = false) {
    if (query.empty()) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            society_only ? "F9 / SOCIETY SEARCH" : "LIBRARY SEARCH");
        query = prompt(society_only ? "SOCIETY SEARCH> " : "SEARCH> ", ansi_enabled);
    }
    auto hits = library.search(query, society_only ? 100 : 12);
    if (society_only) {
        std::erase_if(hits, [&](const SearchHit& hit) {
            return !society_category(library.documents()[hit.document_index].category);
        });
        if (hits.size() > 12) hits.resize(12);
    }
    if (hits.empty()) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            society_only ? "F9 / SOCIETY SEARCH" : "LIBRARY SEARCH");
        paint(ansi_enabled, ansi::red);
        std::cout << "NO LOCAL TEXT MATCHES - TRY FEWER OR MORE SPECIFIC TERMS.\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    std::vector<std::string> items;
    items.reserve(hits.size());
    for (const auto& hit : hits) {
        items.push_back(
            library.documents()[hit.document_index].title + "  //  SOURCE PAGE " +
            std::to_string(hit.page));
    }
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            society_only ? "F9 / SOCIETY SEARCH" : "LIBRARY SEARCH");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "QUERY  " << query << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows(22);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::amber);
        std::cout << "SELECTED PASSAGE\n";
        paint(ansi_enabled, ansi::reset);
        const auto preview = pager_lines(hits[state.selected].snippet, 96);
        const auto preview_rows = std::min<std::size_t>(3, preview.size());
        for (auto row = std::size_t{}; row < preview_rows; ++row) std::cout << preview[row] << '\n';
        for (auto row = preview_rows; row < 3; ++row) std::cout << '\n';
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN  |  Q BACK  |  :OPEN <N>\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("RESULT> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            reader(library, hits[*choice].document_index, hits[*choice].page,
                   profile, ollama_ready, ansi_enabled);
        }
    }
}

std::string inventory_number(const double value) {
    std::ostringstream output;
    if (std::abs(value - std::round(value)) < 0.001) output << static_cast<long long>(std::llround(value));
    else if (std::abs(value) < 1.0) output << std::fixed << std::setprecision(2) << value;
    else output << std::fixed << std::setprecision(1) << value;
    return output.str();
}

std::string command_center_cell(std::string value, const std::size_t cell_width) {
    if (value.size() > cell_width) {
        value = cell_width > 3 ? value.substr(0, cell_width - 3) + "..." : value.substr(0, cell_width);
    }
    if (value.size() < cell_width) value.append(cell_width - value.size(), ' ');
    return value;
}

enum class WorkstationFocus {
    Navigation,
    FieldIo,
    ArchiveSearch,
    GuideQuery,
    Schematics,
    DisplayMode,
};

WorkstationFocus next_workstation_focus(const WorkstationFocus focus, const bool reverse) {
    if (reverse) {
        if (focus == WorkstationFocus::Navigation) return WorkstationFocus::DisplayMode;
        if (focus == WorkstationFocus::FieldIo) return WorkstationFocus::Navigation;
        if (focus == WorkstationFocus::ArchiveSearch) return WorkstationFocus::FieldIo;
        if (focus == WorkstationFocus::GuideQuery) return WorkstationFocus::ArchiveSearch;
        if (focus == WorkstationFocus::Schematics) return WorkstationFocus::GuideQuery;
        return WorkstationFocus::Schematics;
    }
    if (focus == WorkstationFocus::Navigation) return WorkstationFocus::FieldIo;
    if (focus == WorkstationFocus::FieldIo) return WorkstationFocus::ArchiveSearch;
    if (focus == WorkstationFocus::ArchiveSearch) return WorkstationFocus::GuideQuery;
    if (focus == WorkstationFocus::GuideQuery) return WorkstationFocus::Schematics;
    if (focus == WorkstationFocus::Schematics) return WorkstationFocus::DisplayMode;
    return WorkstationFocus::Navigation;
}

void workstation_cell(
    const std::string& value, const std::size_t cell_width,
    const bool selected, const bool ansi_enabled) {
    if (selected) paint(ansi_enabled, ansi::amber_bar);
    else paint(ansi_enabled, ansi::reset);
    std::cout << command_center_cell(value, cell_width);
    paint(ansi_enabled, ansi::reset);
}

void render_workstation_fields(
    const WorkstationFocus focus, const std::string& archive_query,
    const std::string& guide_query, const bool minimal_mode,
    const bool ansi_enabled) {
    const auto panel_width = interface_width();
    const auto tab_inner = panel_width - 7;
    const auto tab_base = tab_inner / 6;
    const std::array<std::size_t, 6> tab_widths{
        tab_base + tab_inner % 6, tab_base, tab_base, tab_base, tab_base, tab_base};
    paint(ansi_enabled, ansi::cyan);
    std::cout << '+' << std::string(panel_width - 2, '=') << "+\n|";
    workstation_cell(
        minimal_mode ? " [1] MENU" : " [1] NAVIGATION", tab_widths[0],
        focus == WorkstationFocus::Navigation, ansi_enabled);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    workstation_cell(
        minimal_mode ? " [2] I/O" : " [2] FIELD I/O // UART + BLE",
        tab_widths[1], focus == WorkstationFocus::FieldIo, ansi_enabled);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    workstation_cell(
        minimal_mode ? " [3] FIND" :
            " [3] FIND> " + (archive_query.empty()
                ? std::string("TYPE TO SEARCH LOCAL ARCHIVE") : archive_query),
        tab_widths[2],
        focus == WorkstationFocus::ArchiveSearch, ansi_enabled);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    workstation_cell(
        minimal_mode ? " [4] ASK" :
            " [4] ASK> " + (guide_query.empty()
                ? std::string("TYPE AN EVIDENCE-FIRST QUESTION") : guide_query),
        tab_widths[3],
        focus == WorkstationFocus::GuideQuery, ansi_enabled);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    workstation_cell(
        minimal_mode ? " [5] SCHEM" : " [5] SCHEMATICS // TEXT",
        tab_widths[4], focus == WorkstationFocus::Schematics, ansi_enabled);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    workstation_cell(
        minimal_mode ? " [6] FULL" : " [6] MINIMAL", tab_widths[5],
        focus == WorkstationFocus::DisplayMode, ansi_enabled);
    paint(ansi_enabled, ansi::cyan);
    std::cout << "|\n";
    std::cout << '+' << std::string(panel_width - 2, '-') << "+\n";
    paint(ansi_enabled, ansi::reset);
}

void render_highlight_bar(
    const double value, const std::size_t cells, const bool ansi_enabled) {
    const auto filled = static_cast<std::size_t>(std::lround(
        std::clamp(value, 0.0, 100.0) * static_cast<double>(cells) / 100.0));
    paint(ansi_enabled, ansi::white);
    std::cout << '[';
    paint(ansi_enabled, ansi::green);
    std::cout << std::string(filled, '#');
    paint(ansi_enabled, ansi::muted);
    std::cout << std::string(cells - filled, '.');
    paint(ansi_enabled, ansi::white);
    std::cout << ']';
}

void render_workstation_graphics(
    const InventoryState& inventory, const InventorySummary& summary,
    const bool ansi_enabled) {
    const auto water_percent = std::clamp(summary.potable_water_liters / 20.0 * 100.0, 0.0, 100.0);
    const auto food_percent = std::clamp(summary.food_meals / 30.0 * 100.0, 0.0, 100.0);
    paint(ansi_enabled, ansi::amber);
    std::cout << "OPS GRAPH  ";
    paint(ansi_enabled, ansi::white);
    std::cout << "READY ";
    render_highlight_bar(summary.readiness_percent, 12, ansi_enabled);
    std::cout << "  H2O ";
    render_highlight_bar(water_percent, 12, ansi_enabled);
    std::cout << "  FOOD ";
    render_highlight_bar(food_percent, 12, ansi_enabled);
    std::cout << "  HEALTH ";
    render_highlight_bar(inventory.health_percent, 10, ansi_enabled);
    constexpr std::size_t visible_width = 11 + 6 + 14 + 6 + 14 + 7 + 14 + 9 + 12;
    if (visible_width < interface_width()) {
        std::cout << std::string(interface_width() - visible_width, ' ');
    }
    std::cout << '\n';
    paint(ansi_enabled, ansi::reset);
}

struct CompanionRenderCache {
    std::filesystem::path asset;
    TerminalColorMode color_mode{TerminalColorMode::Plain};
    std::size_t columns{};
    std::size_t rows{};
    ImageInfo info;
    std::vector<std::string> lines;
    std::string error;
};

const CompanionRenderCache& companion_render(
    const OperatorProfile& profile, const bool ansi_enabled,
    const std::size_t columns, const std::size_t rows) {
    static CompanionRenderCache cache;
    const auto asset = mascot_asset(resource_root(), profile.incident);
    const auto mode = terminal_color_mode(ansi_enabled);
    if (cache.asset == asset && cache.color_mode == mode &&
        cache.columns == columns && cache.rows == rows && !cache.lines.empty()) return cache;

    cache = {};
    cache.asset = asset;
    cache.color_mode = mode;
    cache.columns = columns;
    cache.rows = rows;
    if (!asset.empty() && image_support_available()) {
        std::ostringstream rendered;
        if (render_image_ansi(
                asset, columns, rows, ansi_enabled, rendered,
                cache.info, cache.error)) {
            std::istringstream stream(rendered.str());
            std::string line;
            while (std::getline(stream, line)) cache.lines.push_back(std::move(line));
        }
    } else {
        cache.error = "MASCOT IMAGE PIPELINE UNAVAILABLE";
    }
    while (cache.lines.size() < rows) cache.lines.emplace_back(columns, ' ');
    return cache;
}

void render_command_center_grid(
    const std::vector<std::string>& items, VimMenuState& state,
    const OperatorProfile& profile, const bool ansi_enabled) {
    const auto panel_width = interface_width();
    const auto right_width = std::clamp<std::size_t>(panel_width / 3, 40, 58);
    const auto left_width = panel_width - right_width - 3;
    const auto terminal = terminal_size();
    // Reserve the shell header, complete health/inventory board, action line, and prompt first.
    // The companion then takes the largest portrait viewport that cannot push those below screen.
    const auto image_rows = std::clamp<std::size_t>(
        terminal.rows > 25 ? terminal.rows - 25 : items.size(), items.size(), 16);
    const auto image_columns = std::min<std::size_t>(
        right_width - 2, std::max<std::size_t>(14,
            static_cast<std::size_t>(std::lround(image_rows * 1.68))));
    const auto image_left_padding = right_width - image_columns;
    constexpr std::size_t image_right_padding = 0;

    clamp_vim_menu(state, items.size(), items.size());
    const auto companion_mode = ui_settings().companion_render;
    const auto inline_protocol = terminal_inline_image_protocol();
    const auto inline_asset = mascot_asset(resource_root(), profile.incident);
    const bool inline_companion = companion_mode == "auto" &&
        inline_protocol != TerminalInlineImageProtocol::None && !inline_asset.empty();
    const bool companion_off = companion_mode == "off";
    const CompanionRenderCache* companion = nullptr;
    if (!inline_companion && !companion_off) {
        companion = &companion_render(profile, ansi_enabled, image_columns, image_rows);
    }
    const std::string companion_heading = companion_off
        ? " WAYKEEPER // OFF" : inline_companion
            ? " WAYKEEPER // RGB32 INLINE" : " WAYKEEPER // ANSI";

    paint(ansi_enabled, ansi::cyan);
    std::cout << '+' << std::string(left_width, '=') << '+'
              << std::string(right_width, '=') << "+\n|";
    paint(ansi_enabled, ansi::amber_bar);
    std::cout << command_center_cell(" FUNCTION KEYS", left_width);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    paint(ansi_enabled, ansi::boot_bar);
    std::cout << command_center_cell(companion_heading, right_width);
    paint(ansi_enabled, ansi::cyan);
    std::cout << "|\n+" << std::string(left_width, '-') << '+'
              << std::string(right_width, '-') << "+\n";

    for (std::size_t row = 0; row < image_rows; ++row) {
        paint(ansi_enabled, ansi::cyan);
        std::cout << '|';
        if (row < items.size()) {
            std::ostringstream item;
            item << (row == state.selected ? "> [" : "  [") << std::setw(2) << row + 1
                 << "] " << items[row];
            if (row == state.selected) paint(ansi_enabled, ansi::cyan_bar);
            else paint(ansi_enabled, ansi::reset);
            std::cout << command_center_cell(item.str(), left_width);
            paint(ansi_enabled, ansi::reset);
        } else {
            std::cout << std::string(left_width, ' ');
        }
        paint(ansi_enabled, ansi::cyan);
        std::cout << '|';
        paint(ansi_enabled, ansi::reset);
        std::cout << std::string(image_left_padding, ' ');
        if (inline_companion) {
            if (row == 0) {
                (void)emit_terminal_inline_image(
                    inline_protocol, inline_asset, image_columns, image_rows, ansi_enabled);
            }
            std::cout << std::string(image_columns, ' ');
        } else if (companion_off) {
            if (row == image_rows / 2) {
                std::cout << command_center_cell("COMPANION OFF", image_columns);
            } else {
                std::cout << std::string(image_columns, ' ');
            }
        } else if (!companion || (!companion->error.empty() && row == image_rows / 2)) {
            std::cout << command_center_cell("MASCOT OFFLINE", image_columns);
        } else {
            std::cout << companion->lines[row];
        }
        std::cout << std::string(image_right_padding, ' ');
        paint(ansi_enabled, ansi::cyan);
        std::cout << "|\n";
    }
    std::cout << '+' << std::string(left_width, '-') << '+'
              << std::string(right_width, '-') << "+\n";
    paint(ansi_enabled, ansi::cyan);
    std::string status = "WayTerm (TM) | ITEM " + std::to_string(state.selected + 1) +
        "/" + std::to_string(items.size()) + " | COMPANION " +
        compact_slashes(upper(mascot_mode(profile.incident))) +
        (companion_off ? " / OFF" : inline_companion ? " / RGB32 INLINE" : " / ANSI") +
        " | WayKeeper (TM) | ARROWS MOVE | ENTER OPEN | 0-9 | F1-F12";
    expanded_line(status, panel_width);
    paint(ansi_enabled, ansi::reset);
}

void render_minimal_command_center(
    const std::vector<std::string>& items, VimMenuState& state,
    const bool ansi_enabled) {
    constexpr std::size_t visible_rows = 8;
    const auto panel_width = interface_width();
    clamp_vim_menu(state, items.size(), visible_rows);
    const auto end = std::min(items.size(), state.offset + visible_rows);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '+' << std::string(panel_width - 2, '=') << "+\n|";
    paint(ansi_enabled, ansi::amber_bar);
    std::cout << command_center_cell(" FIELD MENU // CORE + ADVANCED", panel_width - 2);
    paint(ansi_enabled, ansi::cyan);
    std::cout << "|\n+" << std::string(panel_width - 2, '-') << "+\n";
    for (auto index = state.offset; index < end; ++index) {
        std::ostringstream item;
        item << (index == state.selected ? "> [" : "  [") << std::setw(2) << index + 1
             << "] " << items[index];
        std::cout << '|';
        if (index == state.selected) paint(ansi_enabled, ansi::cyan_bar);
        else paint(ansi_enabled, ansi::reset);
        std::cout << command_center_cell(item.str(), panel_width - 2);
        paint(ansi_enabled, ansi::cyan);
        std::cout << "|\n";
    }
    for (auto row = end - state.offset; row < visible_rows; ++row) {
        std::cout << '|' << std::string(panel_width - 2, ' ') << "|\n";
    }
    std::cout << '+' << std::string(panel_width - 2, '-') << "+\n";
    paint(ansi_enabled, ansi::muted);
    expanded_line(
        "ITEM " + std::to_string(state.selected + 1) + "/" + std::to_string(items.size()) +
        "  |  ARROWS MOVE  |  ENTER OPEN  |  TAB FOCUS  |  [6] FULL");
    paint(ansi_enabled, ansi::reset);
}

void render_compact_command_center(
    const std::vector<std::string>& items, VimMenuState& state,
    const bool ansi_enabled) {
    const auto panel_width = interface_width();
    const auto inner_width = panel_width - 3;
    const auto left_width = inner_width / 2;
    const auto right_width = inner_width - left_width;
    constexpr std::size_t rows = 6;
    clamp_vim_menu(state, items.size(), items.size());

    paint(ansi_enabled, ansi::cyan);
    std::cout << '+' << std::string(left_width, '=') << '+'
              << std::string(right_width, '=') << "+\n";
    for (std::size_t row = 0; row < rows; ++row) {
        const auto render_item = [&](const std::size_t index, const std::size_t cell_width) {
            std::ostringstream item;
            item << (index == state.selected ? "> " : "  ") << items[index];
            if (index == state.selected) paint(ansi_enabled, ansi::cyan_bar);
            else paint(ansi_enabled, ansi::reset);
            std::cout << command_center_cell(item.str(), cell_width);
            paint(ansi_enabled, ansi::cyan);
        };
        std::cout << '|';
        render_item(row, left_width);
        std::cout << '|';
        render_item(row + rows, right_width);
        std::cout << "|\n";
    }
    std::cout << '+' << std::string(left_width, '-') << '+'
              << std::string(right_width, '-') << "+\n";
    paint(ansi_enabled, ansi::muted);
    expanded_line(
        "ITEM " + std::to_string(state.selected + 1) + "/" +
        std::to_string(items.size()) + " | ARROWS | ENTER | 0-9 | F1-F12 | TAB TOOLS");
    paint(ansi_enabled, ansi::reset);
}

void render_minimal_status(
    const InventoryState& inventory, const InventorySummary& summary,
    const bool ansi_enabled) {
    const auto network = inspect_network();
    paint(ansi_enabled, ansi::amber);
    expanded_line(
        "HP " + inventory_number(inventory.health_percent) + "%  |  H2O " +
        inventory_number(summary.potable_water_liters) + "L  |  FOOD " +
        inventory_number(summary.food_meals) + "  |  BAT " +
        inventory_number(inventory.battery_percent) + "%  |  SOL " +
        inventory_number(inventory.solar_watts) + "W  |  RAD " +
        inventory_number(inventory.radiation_msv_h) + "  |  NET " +
        (network.online ? "ON" : "OFF"));
    paint(ansi_enabled, summary.alerts.empty() ? ansi::green : ansi::red);
    expanded_line(
        "READY " + std::to_string(summary.readiness_percent) + "%  |  ALERT " +
        std::to_string(summary.alerts.size()) + "  |  F10 INVENTORY  |  F5 SYSTEM");
    paint(ansi_enabled, ansi::reset);
}

std::string inventory_gauge(const double value, const std::size_t cells = 10) {
    const auto filled = static_cast<std::size_t>(std::round(
        std::clamp(value, 0.0, 100.0) / 100.0 * static_cast<double>(cells)));
    return "[" + std::string(filled, '#') + std::string(cells - filled, '-') + "]";
}

std::string inventory_cell(std::string value, const std::size_t cell_width = 31) {
    if (value.size() > cell_width) value = value.substr(0, cell_width - 3) + "...";
    if (value.size() < cell_width) value.append(cell_width - value.size(), ' ');
    return value;
}

std::array<std::size_t, 3> inventory_panel_widths() {
    const auto inner = interface_width() - 4;
    constexpr std::size_t detailed_left = 53;
    constexpr std::size_t detailed_middle = 62;
    constexpr std::size_t detailed_right = 51;
    constexpr std::size_t detailed_total =
        detailed_left + detailed_middle + detailed_right;
    if (inner >= detailed_total) {
        const auto extra = inner - detailed_total;
        const auto left = detailed_left + extra / 3;
        const auto middle = detailed_middle + extra / 3;
        return {left, middle, inner - left - middle};
    }
    const auto base = inner / 3;
    return {base, base, inner - base * 2};
}

void inventory_panel_row(
    const std::string& left, const std::string& middle, const std::string& right,
    const bool ansi_enabled, const char* left_color = ansi::green,
    const char* middle_color = ansi::amber, const char* right_color = ansi::green) {
    const auto cell_widths = inventory_panel_widths();
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    paint(ansi_enabled, left_color);
    std::cout << inventory_cell(left, cell_widths[0]);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    paint(ansi_enabled, middle_color);
    std::cout << inventory_cell(middle, cell_widths[1]);
    paint(ansi_enabled, ansi::cyan);
    std::cout << '|';
    paint(ansi_enabled, right_color);
    std::cout << inventory_cell(right, cell_widths[2]);
    paint(ansi_enabled, ansi::cyan);
    std::cout << "|\n";
    paint(ansi_enabled, ansi::reset);
}

void render_inventory_dashboard(
    const InventoryState& inventory, const InventorySummary& summary,
    const bool ansi_enabled) {
    const auto network = inspect_network();
    inventory_panel_row(
        " OPERATOR / SIM", " FIELD INVENTORY / SIM", " POWER + SENSORS / SIM",
        ansi_enabled, ansi::status_gold_bar, ansi::status_green_bar, ansi::status_gold_bar);
    inventory_panel_row(
        " HP" + inventory_number(inventory.health_percent) + "% | RDY" +
            std::to_string(summary.readiness_percent) + "% | AL" +
            std::to_string(summary.alerts.size()) + " | LIMB OK | NET " +
            (network.online ? "ON/" + network.interface_name : "OFF"),
        " H2O" + inventory_number(summary.potable_water_liters) + "L | FD" +
            inventory_number(summary.food_meals) + " | MED" + inventory_number(summary.medkits) +
            " | BND" + inventory_number(summary.bandages) + " | SUP" +
            std::to_string(summary.supply_item_types) + " | SH" +
            inventory_number(summary.shelter_units) + " | AM" +
            inventory_number(summary.ammo_rounds),
        " BAT" + inventory_number(inventory.battery_percent) + "% | SOL" +
            inventory_number(inventory.solar_watts) + "W | TMP" +
            inventory_number(inventory.temperature_c) + "C | CELL" +
            inventory_number(summary.charged_battery_cells) + "/" +
            inventory_number(summary.battery_cells) + " | RAD" +
            inventory_number(inventory.radiation_msv_h),
        ansi_enabled, summary.alerts.empty() && network.online ? ansi::green : ansi::red,
        ansi::amber, ansi::green);
}

std::optional<double> inventory_number_prompt(
    const std::string_view label, const double current, const bool ansi_enabled) {
    const auto value = prompt(
        std::string(label) + " [" + inventory_number(current) + "]> ", ansi_enabled);
    if (value.empty()) return current;
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stod(value, &consumed);
        if (consumed == value.size() && std::isfinite(parsed) && parsed >= 0.0) return parsed;
    } catch (...) {}
    return std::nullopt;
}

std::string inventory_slug(std::string value) {
    value = lower(value);
    std::string slug;
    bool separator = false;
    for (const unsigned char character : value) {
        if (std::isalnum(character)) {
            slug.push_back(static_cast<char>(character));
            separator = false;
        } else if (!slug.empty() && !separator) {
            slug.push_back('-');
            separator = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug.empty() ? "inventory-item" : slug;
}

void inventory_sensor_screen(
    InventoryState& inventory, const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    VimMenuState state;
    for (;;) {
        std::vector<std::pair<std::string, double*>> sensors{
            {"OPERATOR HEALTH / %", &inventory.health_percent},
            {"BATTERY STATE OF CHARGE / %", &inventory.battery_percent},
            {"SOLAR INPUT / W", &inventory.solar_watts},
            {"BATTERY BUS / V", &inventory.battery_voltage},
            {"AMBIENT TEMPERATURE / C", &inventory.temperature_c},
            {"RELATIVE HUMIDITY / %", &inventory.humidity_percent},
            {"RADIATION / mSv/h", &inventory.radiation_msv_h},
        };
        std::vector<std::string> items;
        for (const auto& sensor : sensors) {
            items.push_back(sensor.first + "  //  " + inventory_number(*sensor.second));
        }
        shell_header(profile, ollama_ready, documents, ansi_enabled, "INVENTORY / SENSOR SIMULATOR");
        paint(ansi_enabled, ansi::cyan_bar);
        std::cout << " SIMULATION INPUTS // NOT CONNECTED TO FIELD HARDWARE ";
        paint(ansi_enabled, ansi::reset);
        std::cout << '\n';
        rule();
        const auto rows = vim_menu_rows(18);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER EDIT SELECTED SENSOR  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("SENSORS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            const auto value = inventory_number_prompt(sensors[*choice].first, *sensors[*choice].second, ansi_enabled);
            if (value) {
                *sensors[*choice].second = *value;
                if (*choice == 0 || *choice == 1 || *choice == 5) {
                    *sensors[*choice].second = std::clamp(*sensors[*choice].second, 0.0, 100.0);
                }
            }
        }
    }
}

void inventory_close_screen(
    InventoryStore& store, InventoryState& inventory, InventorySummary& summary,
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    std::string save_error;
    const bool state_saved = store.save(inventory, save_error);
    summary = summarize_inventory(inventory);
    std::string summary_error;
    const bool summary_saved = store.write_summary(inventory, summary, summary_error);
    shell_header(profile, ollama_ready, documents, ansi_enabled, "INVENTORY CLOSE SUMMARY");
    paint(ansi_enabled, ansi::cyan_bar);
    std::cout << " SIMULATION READINESS SNAPSHOT GENERATED ";
    paint(ansi_enabled, ansi::reset);
    std::cout << "  " << summary.generated_at << '\n';
    rule();
    std::cout << "READINESS   " << inventory_gauge(summary.readiness_percent, 20) << ' '
              << summary.readiness_percent << "%\n"
              << "H2O         " << inventory_number(summary.potable_water_liters) << " L POTABLE\n"
              << "FOOD        " << inventory_number(summary.food_meals) << " MEALS\n"
              << "MEDICAL     " << inventory_number(summary.medkits) << " MEDKITS / "
              << inventory_number(summary.bandages) << " DRESSINGS\n"
              << "SUPPLIES    " << summary.supply_item_types << " STOCKED ITEM TYPES\n"
              << "SHELTER     " << inventory_number(summary.shelter_units) << " TRACKED UNITS\n"
              << "AMMO        " << inventory_number(summary.ammo_rounds) << " ROUNDS / COUNT ONLY\n"
              << "POWER       " << inventory_number(summary.charged_battery_cells) << "/"
              << inventory_number(summary.battery_cells) << " CELLS CHARGED / "
              << inventory_number(inventory.battery_percent) << "% BATTERY\n"
              << "COORDINATES " << inventory.coordinates << "\n";
    if (!summary.alerts.empty()) {
        paint(ansi_enabled, ansi::red);
        for (const auto& alert : summary.alerts) std::cout << "ALERT       " << alert << '\n';
        paint(ansi_enabled, ansi::reset);
    } else {
        paint(ansi_enabled, ansi::green);
        std::cout << "ALERTS      NONE IN CURRENT SIMULATION\n";
        paint(ansi_enabled, ansi::reset);
    }
    rule();
    paint(ansi_enabled, state_saved && summary_saved ? ansi::green : ansi::red);
    if (state_saved && summary_saved) {
        std::cout << "STATE SAVED  " << store.path() << '\n'
                  << "REPORT SAVED " << store.summary_path() << '\n';
    } else {
        std::cout << "INVENTORY WRITE FAULT // "
                  << (!save_error.empty() ? save_error : summary_error) << '\n';
    }
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
}

void inventory_screen(
    InventoryStore& store, InventoryState& inventory, InventorySummary& summary,
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    VimMenuState state;
    for (;;) {
        std::vector<std::string> items;
        for (const auto& item : inventory.items) {
            std::ostringstream row;
            row << std::left << std::setw(10) << item.category << " | "
                << std::setw(29) << item.name << " | "
                << std::right << std::setw(7) << inventory_number(item.quantity) << ' '
                << std::left << std::setw(10) << item.unit << " | LOW "
                << inventory_number(item.low_threshold);
            items.push_back(row.str());
        }
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F10 / INVENTORY");
        paint(ansi_enabled, ansi::cyan_bar);
        std::cout << " FIELD LOADOUT REGISTER // SIMULATION DATA ONLY ";
        paint(ansi_enabled, ansi::reset);
        std::cout << "  POS " << inventory.coordinates << '\n';
        rule();
        const auto rows = vim_menu_rows(18);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/E QTY  |  A ADD  |  D DELETE  |  C COORDINATES  |  S SENSORS\n"
                     "        R RESET SIMULATION  |  Q CLOSE + GENERATE SUMMARY\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("INVENTORY> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") {
            inventory_close_screen(
                store, inventory, summary, profile, ollama_ready, documents, ansi_enabled);
            return;
        }
        if (command == "a" || command == "add") {
            InventoryItem item;
            item.category = upper(prompt("CATEGORY [SUPPLIES]> ", ansi_enabled));
            if (item.category.empty()) item.category = "SUPPLIES";
            item.name = prompt("ITEM NAME> ", ansi_enabled);
            if (item.name.empty()) continue;
            item.id = inventory_slug(item.name);
            const auto base_id = item.id;
            for (std::size_t suffix = 2; std::any_of(
                     inventory.items.begin(), inventory.items.end(), [&](const auto& existing) {
                         return existing.id == item.id;
                     }); ++suffix) item.id = base_id + "-" + std::to_string(suffix);
            if (const auto quantity = inventory_number_prompt("QUANTITY", 1.0, ansi_enabled)) item.quantity = *quantity;
            else continue;
            item.unit = prompt("UNIT [units]> ", ansi_enabled);
            if (item.unit.empty()) item.unit = "units";
            if (const auto threshold = inventory_number_prompt("LOW ALERT THRESHOLD", 0.0, ansi_enabled)) item.low_threshold = *threshold;
            else continue;
            item.note = prompt("NOTE [SIMULATED]> ", ansi_enabled);
            inventory.items.push_back(std::move(item));
            state.selected = inventory.items.size() - 1;
            continue;
        }
        if ((command == "d" || command == "delete") && !inventory.items.empty()) {
            const auto confirmation = lower(prompt(
                "DELETE " + inventory.items[state.selected].name + "? TYPE YES> ", ansi_enabled));
            if (confirmation == "yes") {
                inventory.items.erase(inventory.items.begin() + static_cast<std::ptrdiff_t>(state.selected));
                if (state.selected > 0 && state.selected >= inventory.items.size()) --state.selected;
            }
            continue;
        }
        if (command == "c" || command == "coordinates") {
            const auto coordinates = prompt(
                "COORDINATES / GRID [" + inventory.coordinates + "]> ", ansi_enabled);
            if (!coordinates.empty()) inventory.coordinates = coordinates;
            continue;
        }
        if (command == "s" || command == "sensors") {
            inventory_sensor_screen(inventory, profile, ollama_ready, documents, ansi_enabled);
            continue;
        }
        if (command == "r" || command == "reset") {
            if (upper(prompt("TYPE RESET TO RESTORE THE SIMULATION LOADOUT> ", ansi_enabled)) == "RESET") {
                inventory = default_inventory_state();
                state = {};
            }
            continue;
        }
        if (command == "e" || command == "edit") {
            if (items.empty()) continue;
            const auto value = inventory_number_prompt(
                "NEW QUANTITY FOR " + inventory.items[state.selected].name,
                inventory.items[state.selected].quantity, ansi_enabled);
            if (value) inventory.items[state.selected].quantity = *value;
            continue;
        }
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            const auto value = inventory_number_prompt(
                "NEW QUANTITY FOR " + inventory.items[*choice].name,
                inventory.items[*choice].quantity, ansi_enabled);
            if (value) inventory.items[*choice].quantity = *value;
        }
    }
}

void document_library_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    std::vector<std::size_t> document_indices;
    std::vector<std::string> items;
    items.reserve(library.documents().size());
    for (std::size_t index = 0; index < library.documents().size(); ++index) {
        const auto& document = library.documents()[index];
        if (document.category.starts_with("Archive-") ||
            document.category.starts_with("Cookbook-Underground-Restricted")) continue;
        document_indices.push_back(index);
        std::ostringstream item;
        item << std::left << std::setw(66) << document.title << std::right
             << std::setw(4) << document.pages << " PP  " << upper(document.category);
        items.push_back(item.str());
    }
    VimMenuState state;
    for (;;) {
        shell_header(profile, ollama_ready, library.documents().size(), ansi_enabled, "F2 / DOCUMENT LIBRARY");
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN  |  S SEARCH  |  Q BACK  |  :COMMAND\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("LIBRARY> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command.rfind("search ", 0) == 0) {
            search_screen(library, profile, ollama_ready, ansi_enabled, command.substr(7));
            continue;
        }
        if (command == "search" || command == "s" || command == "/") {
            search_screen(library, profile, ollama_ready, ansi_enabled);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            reader(library, document_indices[*choice], 1, profile, ollama_ready, ansi_enabled);
        }
    }
}

void cookbook_index_search_screen(
    SurvivalLibrary& library, const std::size_t document_index,
    const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    const auto query = prompt("RESTRICTED INDEX SEARCH> ", ansi_enabled);
    if (query.empty()) return;
    const auto hits = library.search_document(document_index, query, 100);
    std::vector<std::string> items;
    items.reserve(hits.size());
    for (const auto& hit : hits) {
        items.push_back("RESTRICTED MATCH // SOURCE PAGE " + std::to_string(hit.page));
    }
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / COOKBOOK / RESTRICTED LOCAL INDEX");
        paint(ansi_enabled, ansi::red);
        std::cout << "AGE RESTRICTED 18+ // ILLEGAL CONTENT - FOR REFERENCE PURPOSES ONLY - HISTORIC\n"
                     "LOCAL INDEX ONLY // EXCLUDED FROM GUIDE + DEEPSEARCH // SNIPPETS SUPPRESSED\n";
        paint(ansi_enabled, ansi::cyan);
        std::cout << "QUERY " << query << "  |  " << hits.size() << " PAGE MATCHES\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        if (items.empty()) {
            paint(ansi_enabled, ansi::muted);
            std::cout << "NO MATCHES IN THE RESTRICTED LOCAL INDEX.\n";
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
            return;
        }
        const auto rows = vim_menu_rows(17);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN GATED SOURCE PAGE  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("COOKBOOK INDEX> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            reader(
                library, document_index, hits[*choice].page,
                profile, ollama_ready, ansi_enabled, true);
        }
    }
}

void cookbook_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    std::vector<std::size_t> document_indices;
    std::vector<std::string> items;
    for (std::size_t index = 0; index < library.documents().size(); ++index) {
        const auto& document = library.documents()[index];
        if (!document.category.starts_with("Cookbook-")) continue;
        document_indices.push_back(index);
        std::ostringstream item;
        item << std::left << std::setw(66) << document.title << std::right
             << std::setw(4) << document.pages << " PP  " << upper(document.category);
        items.push_back(item.str());
    }
    VimMenuState state;
    bool access_granted = false;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / PDFS / COOKBOOK");
        paint(ansi_enabled, ansi::red);
        std::cout << "AGE RESTRICTED 18+ // ILLEGAL CONTENT - FOR REFERENCE PURPOSES ONLY - HISTORIC\n"
                     "LOCAL FULL-TEXT INDEX // EXCLUDED FROM GUIDE + DEEPSEARCH\n"
                     "SOURCES MAY BE FALSE, ILLEGAL, VIOLENT, DANGEROUS, OR RIGHTS-ENCUMBERED\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows(17);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN  |  S SEARCH RESTRICTED INDEX  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("COOKBOOK> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if ((command == "s" || command == "search" || command == "/") &&
            !document_indices.empty()) {
            if (!access_granted) access_granted = acknowledge_restricted_cookbook(ansi_enabled);
            if (access_granted) cookbook_index_search_screen(
                library, document_indices.front(), profile, ollama_ready, ansi_enabled);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            if (!access_granted) access_granted = acknowledge_restricted_cookbook(ansi_enabled);
            if (access_granted) reader(
                library, document_indices[*choice], 1,
                profile, ollama_ready, ansi_enabled, true);
        }
    }
}

void technical_document_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled, const std::string_view prefix, const std::string_view heading) {
    std::vector<std::size_t> document_indices;
    std::vector<std::string> items;
    for (std::size_t index = 0; index < library.documents().size(); ++index) {
        const auto& document = library.documents()[index];
        if (!document.category.starts_with(prefix)) continue;
        document_indices.push_back(index);
        std::ostringstream item;
        item << std::left << std::setw(66) << document.title << std::right
             << std::setw(4) << document.pages << " PP  " << upper(document.category);
        items.push_back(item.str());
    }

    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / TECHNICAL WORKSHOP // " + std::string(heading));
        if (items.empty()) {
            paint(ansi_enabled, ansi::red);
            std::cout << "NO OFFLINE SOURCES INSTALLED FOR THIS TECHNICAL SHELF\n";
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
            return;
        }
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN READER  |  Q BACK  |  :COMMAND\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("WORKSHOP READER> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            reader(
                library, document_indices[*choice], 1, profile, ollama_ready, ansi_enabled);
        }
    }
}

void agriculture_library_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    struct Shelf { std::string label; std::string prefix; };
    const std::vector<Shelf> shelves{
        {"SOIL // TESTING + FERTILITY + COMPOST + REMEDIATION", "Agriculture-Soil"},
        {"SEEDS // SAVING + PROCESSING + STORAGE", "Agriculture-Seeds"},
        {"VEGETABLES // PRODUCTION + ROTATIONS", "Agriculture-Vegetables"},
        {"STAPLES // GRAINS + BEANS + POTATOES + CALORIE CROPS", "Agriculture-Staples"},
        {"ORCHARDS // BERRIES + VINES + NUT TREES", "Agriculture-Orchards"},
        {"WATER // IRRIGATION + WELLS + RAIN + DROUGHT", "Agriculture-Water"},
        {"CONTROLLED GROWING // GREENHOUSE + HYDROPONICS + SEASON", "Agriculture-Greenhouse"},
        {"INTEGRATED PEST + DISEASE MANAGEMENT", "Agriculture-IPM"},
        {"FARM TOOLS + SMALL MACHINERY", "Agriculture-Tools"},
        {"LIVESTOCK // POULTRY + RABBITS + RUMINANTS + BEES", "Agriculture-Livestock"},
        {"PASTURE // HAY + FORAGE + FEED STORAGE", "Agriculture-Pasture"},
        {"CROP CALENDARS + USDA HARDINESS ZONES", "Agriculture-Calendar-Zones"},
        {"NEW YORK // PLANTING + HARVEST", "Agriculture-New-York"},
        {"POST-HARVEST // ROOT CELLARS + STORAGE", "Agriculture-Postharvest"},
        {"FARM PLANNING // YIELDS + LABOR + COMMUNITY FOOD", "Agriculture-Planning"},
    };
    std::vector<std::string> items;
    for (const auto& shelf : shelves) {
        const auto count = std::count_if(
            library.documents().begin(), library.documents().end(), [&](const auto& document) {
                return document.category.starts_with(shelf.prefix);
            });
        std::ostringstream item;
        item << std::left << std::setw(76) << shelf.label << std::right << std::setw(3)
             << count << " SOURCES";
        items.push_back(item.str());
    }

    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / AGRICULTURE + HORTICULTURE // 15 FIELD SHELVES");
        paint(ansi_enabled, ansi::red);
        std::cout << "LOCAL CONDITIONS + CURRENT LABELS / LAW / EXTENSION / VETERINARY GUIDANCE CONTROL\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN SHELF  |  S SEARCH LIBRARY  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("AGRICULTURE> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command.rfind("search ", 0) == 0) {
            search_screen(library, profile, ollama_ready, ansi_enabled, command.substr(7));
            continue;
        }
        if (command == "s" || command == "search" || command == "/") {
            search_screen(library, profile, ollama_ready, ansi_enabled);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            technical_document_screen(
                library, profile, ollama_ready, ansi_enabled,
                shelves[*choice].prefix, shelves[*choice].label);
        }
    }
}

void field_shelf_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled, const std::string_view title,
    const std::vector<std::pair<std::string, std::string>>& shelves,
    const std::string_view warning) {
    std::vector<std::string> items;
    for (const auto& [label, prefix] : shelves) {
        const auto count = std::count_if(
            library.documents().begin(), library.documents().end(), [&](const auto& document) {
                return document.category.starts_with(prefix);
            });
        std::ostringstream item;
        item << std::left << std::setw(76) << label << std::right << std::setw(3)
             << count << " SOURCES";
        items.push_back(item.str());
    }
    VimMenuState state;
    for (;;) {
        shell_header(profile, ollama_ready, library.documents().size(), ansi_enabled,
                     "F2 / " + std::string(title));
        paint(ansi_enabled, ansi::red);
        std::cout << warning << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN SHELF  |  S SEARCH LIBRARY  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("FIELD SHELVES> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command == "s" || command == "search" || command == "/") {
            search_screen(library, profile, ollama_ready, ansi_enabled);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            technical_document_screen(
                library, profile, ollama_ready, ansi_enabled,
                shelves[*choice].second, shelves[*choice].first);
        }
    }
}

void prep_library_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    const std::vector<std::pair<std::string, std::string>> shelves{
        {"EMERGENCY + HURRICANE + DISASTER RECOVERY KITS", "Prep-Emergency-Kits"},
        {"DISTRIBUTED KIT CACHES // HOME + WORK + VEHICLE", "Prep-Distributed-Kits"},
        {"LAWFUL FIREARM + AMMUNITION SAFE STORAGE", "Prep-Firearm-Safe-Storage"},
        {"CARTOGRAPHY // MAP READING + SYMBOLS", "Prep-Cartography"},
        {"BUY + ROTATE + STORE // PROCUREMENT FIELD GUIDE", "Prep-Sourcing-Storage"},
    };
    field_shelf_screen(
        library, profile, ollama_ready, ansi_enabled,
        "PRESENT-DAY PREP // BUY + BUILD + ROTATE",
        shelves,
        "CURRENT LOCAL LAW, PRODUCT LABELS, EXPIRATION DATES, AND OFFICIAL ALERTS CONTROL");
}

void zombie_security_library_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    const std::vector<std::pair<std::string, std::string>> shelves{
        {"STRONGHOLD // SAFE ROOM + HOME HARDENING", "Zombie-Stronghold"},
        {"SHELTER-IN // AIR + WATER + FAMILY PROCEDURES", "Zombie-Shelter-In"},
        {"COMPOUND // STRUCTURAL REINFORCEMENT + PARTS", "Zombie-Compound-Reinforcement"},
        {"HOMESTEAD SECURITY // LAWFUL + NON-CONFRONTATIONAL", "Zombie-Homestead-Security"},
    };
    field_shelf_screen(
        library, profile, ollama_ready, ansi_enabled,
        "ZOMBIE DRILL // PRACTICAL HOMESTEAD SECURITY",
        shelves,
        "FICTIONAL ZED SCENARIO // REAL STORM, SHELTER-IN, LOOTING, AND PROPERTY-SAFETY REFERENCES");
}

void online_manual_resource_screen(const bool ansi_enabled) {
    const auto root = resource_root();
    const auto script = root / "scripts" / "manual_resource_fetch.py";
    if (!std::filesystem::exists(script)) {
        paint(ansi_enabled, ansi::red);
        std::cout << "ONLINE MANUAL RESOURCE CONSOLE IS NOT INSTALLED: " << script << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    const std::string shell_command =
        "python3 \"" + script.string() + "\" --interactive --root \"" + root.string() + "\"";
    const auto status = std::system(shell_command.c_str());
    if (status != 0) {
        paint(ansi_enabled, ansi::red);
        std::cout << "MANUAL RESOURCE CONSOLE RETURNED STATUS " << status << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
    }
}

void technical_library_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    struct Branch { std::string label; std::string prefix; };
    const std::vector<Branch> branches{
        {"ELECTRONICS // BASICS", "Electronics-Basics"},
        {"ELECTRONICS // INTERMEDIATE", "Electronics-Intermediate"},
        {"ELECTRONICS // ADVANCED", "Electronics-Advanced"},
        {"AUTOMOTIVE // BASICS + SHOP SAFETY", "Automotive-Basics"},
        {"AUTOMOTIVE // INTERMEDIATE + TRUCKS", "Automotive-Intermediate"},
        {"AUTOMOTIVE // ADVANCED + HEAVY + BUCKET", "Automotive-Advanced"},
        {"PRODUCT MANUAL ARCHIVES // INDEX + CAPACITY", "Product-Manual-Archives"},
        {"AGRICULTURE + HORTICULTURE // 15 FIELD SHELVES", "Agriculture-"},
        {"ONLINE MANUAL RESOURCES // SEARCH + STAGE METADATA", "Online-Manual-Resources"},
    };
    std::vector<std::string> items;
    for (const auto& branch : branches) {
        const auto count = std::count_if(
            library.documents().begin(), library.documents().end(), [&](const auto& document) {
                return document.category.starts_with(branch.prefix);
            });
        std::ostringstream item;
        item << std::left << std::setw(66) << branch.label << std::right << std::setw(4)
             << count << " SOURCES";
        items.push_back(item.str());
    }

    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / TECHNICAL WORKSHOP // REPAIR + AGRICULTURE SYSTEMS");
        paint(ansi_enabled, ansi::red);
        std::cout << "TRAINING REFERENCES // CURRENT EQUIPMENT MANUALS + SAFETY PROCEDURES CONTROL\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN SHELF  |  S SEARCH LIBRARY  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("TECHNICAL> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command.rfind("search ", 0) == 0) {
            search_screen(library, profile, ollama_ready, ansi_enabled, command.substr(7));
            continue;
        }
        if (command == "s" || command == "search" || command == "/") {
            search_screen(library, profile, ollama_ready, ansi_enabled);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            if (branches[*choice].prefix == "Agriculture-") {
                agriculture_library_screen(library, profile, ollama_ready, ansi_enabled);
            } else if (branches[*choice].prefix == "Online-Manual-Resources") {
                online_manual_resource_screen(ansi_enabled);
            } else {
                technical_document_screen(
                    library, profile, ollama_ready, ansi_enabled,
                    branches[*choice].prefix, branches[*choice].label);
            }
        }
    }
}

void text_schematic_viewer(
    const TextSchematic& schematic, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    std::string error;
    const auto source = read_text_schematic(schematic, error);
    if (!source) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "SCHEMATICS / READ FAULT");
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    const auto lines = fixed_schematic_lines(*source);
    const auto diagram_width = fixed_schematic_width(lines);
    std::size_t vertical_offset = 0;
    std::size_t horizontal_offset = 0;
    for (;;) {
        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            "SCHEMATICS / " + upper(schematic.category));
        paint(ansi_enabled, ansi::cyan);
        std::cout << upper(schematic.title) << "\nSOURCE  " << schematic.source_note << '\n';
        paint(ansi_enabled, ansi::red);
        std::cout << "BOUNDARY  " << schematic.safety_note << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();

        const auto geometry = pager_geometry();
        const auto vertical_max = pager_max_offset(lines, geometry.rows);
        const auto horizontal_max = diagram_width > geometry.columns
            ? diagram_width - geometry.columns : 0;
        vertical_offset = std::min(vertical_offset, vertical_max);
        horizontal_offset = std::min(horizontal_offset, horizontal_max);
        const auto end = std::min(lines.size(), vertical_offset + geometry.rows);
        for (auto index = vertical_offset; index < end; ++index) {
            std::cout << fixed_schematic_slice(
                lines[index], horizontal_offset, geometry.columns) << '\n';
        }
        for (auto row = end - vertical_offset; row < geometry.rows; ++row) {
            std::cout << std::string(geometry.columns, ' ') << '\n';
        }
        rule();
        paint(ansi_enabled, ansi::cyan);
        std::cout << "FIXED CELLS  X " << horizontal_offset + 1 << '-' <<
            std::min(diagram_width, horizontal_offset + geometry.columns) << " / " <<
            diagram_width << "  |  Y " << vertical_offset + 1 << '-' << end << " / " <<
            lines.size() << '\n';
        paint(ansi_enabled, ansi::muted);
        std::cout << "PAN  H/L OR LEFT/RIGHT  |  SCROLL J/K SPACE/B PGUP/PGDN  |  0 LEFT  $ RIGHT  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);

        const auto command = pager_prompt("SCHEMATIC> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (pager_scroll(command, vertical_offset, lines, geometry.rows)) continue;
        if (command == " " || command == "space" || command == "pgdn") {
            vertical_offset = std::min(vertical_max, vertical_offset + geometry.rows);
        } else if (command == "b" || command == "pgup") {
            vertical_offset = vertical_offset > geometry.rows
                ? vertical_offset - geometry.rows : 0;
        } else if (command == "h" || command == "left") {
            horizontal_offset = horizontal_offset > 8 ? horizontal_offset - 8 : 0;
        } else if (command == "l" || command == "right") {
            horizontal_offset = std::min(horizontal_max, horizontal_offset + 8);
        } else if (command == "0") {
            horizontal_offset = 0;
        } else if (command == "$") {
            horizontal_offset = horizontal_max;
        }
    }
}

void schematics_screen(
    const std::filesystem::path& root, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    std::vector<TextSchematic> schematics;
    std::string error;
    if (!load_text_schematics(
            root / "library" / "segments" / "Schematics" / "txt" / "catalog.tsv",
            schematics, error)) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "SCHEMATICS / CATALOG FAULT");
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    std::vector<std::string> items;
    for (const auto& schematic : schematics) {
        std::ostringstream item;
        item << std::left << std::setw(78) << schematic.title << upper(schematic.category);
        items.push_back(item.str());
    }
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            "SCHEMATICS // FIXED-CELL TEXT VIEWER");
        paint(ansi_enabled, ansi::cyan);
        std::cout << schematics.size() << " LOCAL TEXT CARDS // NO REFLOW // EIGHT-CELL TAB STOPS\n";
        paint(ansi_enabled, ansi::red);
        std::cout << "ALLOW-LIST ONLY // HISTORICAL RESTRICTED METADATA IS NOT A BUILDABLE SCHEMATIC\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN CARD  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("SCHEMATICS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            text_schematic_viewer(
                schematics[*choice], profile, ollama_ready, documents, ansi_enabled);
        }
    }
}

void restricted_book_archive_screen(
    SurvivalLibrary& library, const OperatorProfile& profile,
    const bool ollama_ready, const bool ansi_enabled) {
    std::vector<std::size_t> document_indices;
    std::vector<std::string> items;
    for (std::size_t index = 0; index < library.documents().size(); ++index) {
        const auto& document = library.documents()[index];
        if (!document.category.starts_with("Archive-")) continue;
        document_indices.push_back(index);
        std::ostringstream item;
        item << std::left << std::setw(66) << document.title << std::right
             << std::setw(4) << document.pages << " PP  " << upper(document.category);
        items.push_back(item.str());
    }
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / HIDDEN BOOK ARCHIVES");
        paint(ansi_enabled, ansi::red);
        std::cout << "ARCHIVE SHELF // EXCLUDED FROM NORMAL DOCUMENT LIST, SEARCH, AND LOCAL GUIDE\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN LOCAL ARCHIVE COPY  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("ARCHIVES> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            reader(
                library, document_indices[*choice], 1,
                profile, ollama_ready, ansi_enabled);
        }
    }
}

void library_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    const std::vector<std::string> items{
        "ALL DOCUMENTS // COMPLETE OFFLINE LIBRARY",
        "TECHNICAL WORKSHOP // REPAIR + AGRICULTURE + ONLINE MANUAL SEARCH",
        "SCHEMATICS // FIXED-CELL TEXT DIAGRAM VIEWER",
        "FICTION + OCCULT LITERATURE // LOVECRAFT",
        "PRESENT-DAY PREP // KITS + STORAGE + CARTOGRAPHY",
        "ZOMBIE DRILL // SHELTER-IN + STRONGHOLD + HOMESTEAD SECURITY",
        "ARCHIVES // HIDDEN AGRICULTURE BOOKS",
        "COOKBOOK // RESTRICTED UNDERGROUND ARCHIVE",
    };
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F2 / PDFS + READER COLLECTIONS");
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN COLLECTION  |  S SEARCH SAFE INDEX  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("PDFS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command.rfind("search ", 0) == 0) {
            search_screen(library, profile, ollama_ready, ansi_enabled, command.substr(7));
            continue;
        }
        if (command == "search" || command == "s" || command == "/") {
            search_screen(library, profile, ollama_ready, ansi_enabled);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            if (*choice == 0) {
                document_library_screen(library, profile, ollama_ready, ansi_enabled);
            } else if (*choice == 1) {
                technical_library_screen(library, profile, ollama_ready, ansi_enabled);
            } else if (*choice == 2) {
                schematics_screen(
                    resource_root(), profile, ollama_ready,
                    library.documents().size(), ansi_enabled);
            } else if (*choice == 3) {
                technical_document_screen(
                    library, profile, ollama_ready, ansi_enabled,
                    "Fiction-Occult-", "FICTION + OCCULT LITERATURE");
            } else if (*choice == 4) {
                prep_library_screen(library, profile, ollama_ready, ansi_enabled);
            } else if (*choice == 5) {
                zombie_security_library_screen(library, profile, ollama_ready, ansi_enabled);
            } else if (*choice == 6) {
                restricted_book_archive_screen(
                    library, profile, ollama_ready, ansi_enabled);
            } else if (*choice == 7) {
                cookbook_screen(library, profile, ollama_ready, ansi_enabled);
            }
        }
    }
}

void society_document_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled, const std::string_view prefix, const std::string_view heading) {
    std::vector<std::size_t> document_indices;
    std::vector<std::string> items;
    for (std::size_t index = 0; index < library.documents().size(); ++index) {
        const auto& document = library.documents()[index];
        if (!document.category.starts_with(prefix)) continue;
        document_indices.push_back(index);
        std::ostringstream item;
        item << std::left << std::setw(66) << document.title << std::right
             << std::setw(4) << document.pages << " PP  " << upper(document.category);
        items.push_back(item.str());
    }

    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F9 / SOCIETY // " + std::string(heading));
        if (items.empty()) {
            paint(ansi_enabled, ansi::red);
            std::cout << "NO READER SOURCES INSTALLED FOR THIS SOCIETY BRANCH\n";
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
            return;
        }
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN READER  |  Q BACK  |  :COMMAND\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("SOCIETY READER> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            reader(
                library, document_indices[*choice], 1, profile, ollama_ready, ansi_enabled);
        }
    }
}

void philosophy_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    struct Branch { std::string label; std::string prefix; };
    const std::vector<Branch> branches{
        {"ETHICS + THE GOOD LIFE", "Philosophy-Ethics-Good-Life"},
        {"GOVERNMENT + POWER", "Philosophy-Government-Power"},
        {"WAR + PEACE", "Philosophy-War-Peace"},
        {"LOGIC + KNOWLEDGE", "Philosophy-Logic-Knowledge"},
        {"MIND + HUMAN NATURE", "Philosophy-Mind-Human-Nature"},
        {"NATURE + SCIENCE", "Philosophy-Nature-Science"},
        {"MEANING + SOLITUDE", "Philosophy-Meaning-Solitude"},
        {"WORLD TRADITIONS", "Philosophy-World-Traditions"},
        {"PHILOSOPHICAL LITERATURE", "Philosophy-Philosophical-Literature"},
        {"MODERN / LICENSE REQUIRED", "Philosophy-Modern-Licensed"},
    };
    std::vector<std::string> items;
    for (const auto& branch : branches) {
        const auto count = std::count_if(
            library.documents().begin(), library.documents().end(), [&](const auto& document) {
                return document.category.starts_with(branch.prefix);
            });
        std::ostringstream item;
        item << std::left << std::setw(66) << branch.label << std::right << std::setw(4)
             << count << " SOURCES";
        items.push_back(item.str());
    }
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F9 / SOCIETY / PHILOSOPHY");
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN SHELF  |  S SEARCH SOCIETY  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("PHILOSOPHY> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command == "s" || command == "search") {
            search_screen(library, profile, ollama_ready, ansi_enabled, {}, true);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            society_document_screen(
                library, profile, ollama_ready, ansi_enabled,
                branches[*choice].prefix, "PHILOSOPHY / " + branches[*choice].label);
        }
    }
}

void society_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const bool ansi_enabled) {
    const std::vector<std::string> items{
        "BELIEF + SACRED TEXTS",
        "CIVICS + FOUNDING PAPERS + GOVERNMENT",
        "PHILOSOPHY // TEN-SHELF OFFLINE READER",
        "FOIA + DECLASSIFIED RECORDS",
        "PRIVACY + SURVEILLANCE // CRITIQUE + DEFENSE",
        "SURVIVAL ECONOMY // BARTER + TRADE + VALUE",
        "REBUILDING SOCIETY // RECOVERY + CONTINUITY",
        "TEXTFILES UNDERGROUND // 33,009-FILE LOCAL ARCHIVE",
    };
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "F9 / SOCIETY // PRESERVE IDEAS + INSTITUTIONS + HUMAN MEMORY");
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER OPEN BRANCH  |  S SEARCH SOCIETY  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("SOCIETY> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command.rfind("search ", 0) == 0) {
            search_screen(library, profile, ollama_ready, ansi_enabled, command.substr(7), true);
            continue;
        }
        if (command == "s" || command == "search") {
            search_screen(library, profile, ollama_ready, ansi_enabled, {}, true);
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            if (*choice == 0) society_document_screen(
                library, profile, ollama_ready, ansi_enabled, "Religion-", "BELIEF + SACRED TEXTS");
            else if (*choice == 1) society_document_screen(
                library, profile, ollama_ready, ansi_enabled, "Civics-", "CIVICS + GOVERNMENT");
            else if (*choice == 2) philosophy_screen(
                library, profile, ollama_ready, ansi_enabled);
            else if (*choice == 3) society_document_screen(
                library, profile, ollama_ready, ansi_enabled, "FOIA-", "FOIA + DECLASSIFIED");
            else if (*choice == 4) society_document_screen(
                library, profile, ollama_ready, ansi_enabled,
                "Privacy-", "PRIVACY + SURVEILLANCE");
            else if (*choice == 5) society_document_screen(
                library, profile, ollama_ready, ansi_enabled, "Economy-", "SURVIVAL ECONOMY + FAIR TRADE");
            else if (*choice == 6) society_document_screen(
                library, profile, ollama_ready, ansi_enabled, "Rebuilding-", "REBUILDING SOCIETY");
            else if (*choice == 7) society_document_screen(
                library, profile, ollama_ready, ansi_enabled, "Underground-TEXTFILES", "TEXTFILES UNDERGROUND ARCHIVE");
        }
    }
}

void guide_screen(
    SurvivalLibrary& library, const OllamaClient& ollama, const OperatorProfile& profile,
    const bool ansi_enabled, std::string question = {}) {
    const bool ready = ollama.server_ready();
    shell_header(profile, ready, library.documents().size(), ansi_enabled, "F3 / LOCAL GUIDE");
    if (question.empty()) question = prompt("ASK> ", ansi_enabled);
    if (question.empty()) return;
    if (const auto card = find_reviewed_card(question)) {
        show_card(*card, profile, ready, library.documents().size(), ansi_enabled);
        return;
    }

    const auto hits = library.search(question, 4);
    if (hits.empty()) {
        paint(ansi_enabled, ansi::red);
        std::cout << "NO REVIEWED CARD OR LOCAL PASSAGE MATCHED.\n"
                     "GUIDE ABORTED - SAFETY ANSWERS REQUIRE LOCAL EVIDENCE.\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    std::ostringstream context;
    paint(ansi_enabled, ansi::cyan);
    std::cout << "\nLOCAL EVIDENCE SET\n";
    paint(ansi_enabled, ansi::reset);
    for (const auto& hit : hits) {
        const auto& document = library.documents()[hit.document_index];
        std::cout << "  > " << document.title << " // SOURCE PAGE " << hit.page << '\n';
        context << "[" << document.title << ", source page " << hit.page << "]\n"
                << hit.snippet << "\n\n";
    }
    if (!ready) {
        paint(ansi_enabled, ansi::red);
        std::cout << (ollama.enabled()
            ? "\nOLLAMA OFFLINE - EVIDENCE RETRIEVAL COMPLETE; INFERENCE SKIPPED.\n"
            : "\nOLLAMA COMPILE-DISABLED - EVIDENCE RETRIEVAL COMPLETE; INFERENCE SKIPPED.\n");
        paint(ansi_enabled, ansi::muted);
        std::cout << (ollama.enabled()
            ? "SETUP COMMAND: scripts/setup_ollama.sh\n"
            : "SOURCE RETAINED // EMBEDDED POLICY PREVENTS MODEL EXECUTION\n");
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    const auto proceed = lower(prompt("\nRUN LOCAL INFERENCE / USE BATTERY? [Y/N]> ", ansi_enabled));
    if (proceed != "y" && proceed != "yes") return;

    const std::string full_prompt =
        "You are the OFF-GRID Survival Guide in a personal ANSI terminal. Current user context: "
        "incident=" + profile.incident + ", terrain=" + profile.terrain + ". Answer only from the "
        "supplied local evidence. Cite document title and source page for every material claim. If "
        "evidence is incomplete, say so. Never override product labels or emergency officials.\n\n"
        "QUESTION:\n" + question + "\n\nLOCAL EVIDENCE:\n" + context.str();
    paint(ansi_enabled, ansi::green);
    std::cout << "\nLOCAL INFERENCE RUNNING // MODEL " << ollama.model() << "\n\n";
    paint(ansi_enabled, ansi::reset);
    std::string error;
    if (const auto answer = ollama.generate(full_prompt, error)) {
        vim_text_screen(
            profile, ready, library.documents().size(), ansi_enabled,
            "F3 / LOCAL GUIDE RESPONSE", "LOCAL MODEL // " + upper(ollama.model()), *answer);
        return;
    } else {
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
    }
    pause(ansi_enabled);
}

void profile_screen(
    OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    const std::vector<std::string> items{
        "EDIT OPERATOR / INCIDENT / TERRAIN",
        "RETURN TO COMMAND CENTER",
    };
    VimMenuState state;
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F4 / OPERATOR PROFILE");
        std::cout << "NAME       " << profile.name << '\n'
                  << "INCIDENT   " << profile.incident << '\n'
                  << "TERRAIN    " << profile.terrain << '\n'
                  << "STORAGE    " << profile_path() << '\n';
        const auto active_mascot = mascot_asset(resource_root(), profile.incident);
        if (!active_mascot.empty() && image_support_available()) {
            ImageInfo image_info;
            std::string image_error;
            render_image_ansi(
                active_mascot, 18, 10, ansi_enabled,
                std::cout, image_info, image_error);
            paint(ansi_enabled, ansi::cyan);
            std::cout << "ACTIVE MASCOT  " << mascot_mode(profile.incident) << '\n';
            paint(ansi_enabled, ansi::reset);
        }
        paint(ansi_enabled, ansi::muted);
        std::cout << "This profile is local context, not location tracking or an official declaration.\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows(20);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER SELECT  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("PROFILE> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            if (*choice == 0) profile = onboarding(ansi_enabled, &profile);
            return;
        }
    }
}

std::optional<std::string> journal_composer(
    const OperatorProfile& profile, bool ollama_ready, std::size_t documents,
    bool ansi_enabled, std::string_view title, std::string_view initial_body = {},
    std::string_view section = "F8 / JOURNAL COMPOSER");

bool edit_last_will_message(
    const OperatorProfile& profile, bool ollama_ready, std::size_t documents,
    bool ansi_enabled);

void system_screen(
    OperatorProfile& profile, const OllamaClient& ollama, const SurvivalLibrary& library,
    const std::filesystem::path& root, TerminalSettings& settings, const bool ansi_enabled) {
    const std::vector<std::string> items{
        "ALTER INCIDENT MODE // SWITCH CONTEXT + MASCOT",
        "TERMINAL PRESET  1440X600 // FIELD-WIDE",
        "TERMINAL PRESET  1600X720 // EXTENDED FIELD",
        "TERMINAL PRESET  1920X800 // ULTRAWIDE",
        "CUSTOM TERMINAL PIXEL SIZE",
        "TOGGLE RESIZE ON RELAUNCH",
        "APPLY SAVED SIZE NOW",
        "CYCLE THEME // BLUE / GOLD / GREEN",
        "SET ROOM CODE // MASCOT BADGE",
        "CYCLE COMMAND CENTER MODEL // WORKSTATION / MINIMAL / STATIC",
        "TOGGLE TOUCH QWERTY ON NEXT BOOT",
        "CYCLE COMPANION RENDER // AUTO / ANSI / OFF",
        "CYCLE SENTINEL MOVIE SPEED // 0.5X / 1.0X / 2.0X",
        "EDIT LAST WILL // FOUND-DEVICE MESSAGE",
        "TOGGLE LAST WILL ON LOCKSCREEN // OPT-IN PUBLIC",
        "LOCK NOW // WAYKEEPER SENTINEL",
        "RETURN TO COMMAND CENTER",
    };
    VimMenuState state;
    for (;;) {
        const bool ready = ollama.server_ready();
        HerbDatabaseStats herb_stats;
        std::string herb_error;
        const bool herbs_ready = inspect_herb_database(
            herb_database_path(root), herb_stats, herb_error);
        std::string will_error;
        const bool will_ready = load_sentinel_last_will(
            sentinel_last_will_path(), will_error).has_value();
        shell_header(profile, ready, library.documents().size(), ansi_enabled, "F5 / SYSTEM + SETTINGS");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "LOCAL SERVICES\n";
        paint(ansi_enabled, ansi::reset);
        std::cout << "  ANSI/VT       " << (ansi_enabled ? "ENABLED" : "PLAIN-TEXT FALLBACK") << '\n'
                  << "  UI MODE       " << upper(settings.layout_mode)
                  << " / INPUT LOCK / FIXED VIEWPORTS\n"
                  << "  INCIDENT      " << upper(profile.incident) << '\n'
                  << "  WINDOW        " << settings.width_pixels << 'X' << settings.height_pixels
                  << " PX  |  RELAUNCH " << (settings.resize_on_launch ? "ON" : "OFF") << '\n'
                  << "  THEME         " << upper(settings.theme) << "  |  ROOM "
                  << settings.room_code << "  |  TOUCH QWERTY "
                  << (settings.touch_keyboard ? "ON" : "OFF") << '\n'
                  << "  COMPANION     " << upper(settings.companion_render) << "  |  "
                  << (terminal_inline_image_protocol() == TerminalInlineImageProtocol::WayTerm
                          ? "WAYTERM RGB32 INLINE HOST" : "ANSI INLINE FALLBACK") << '\n'
                  << "  DEAD-MAN      05:00 REALTIME  |  SENTINEL MOVIE "
                  << movie_speed_label() << "  |  WAKE ESC-W-K-ENTER\n"
                  << "  LAST WILL     "
                  << (settings.sentinel_last_will_enabled ? "PUBLIC / " : "PRIVATE / ")
                  << (will_ready ? "MESSAGE READY" : "NO MESSAGE") << '\n'
                  << "  PDF IMAGE     " << (image_support_available() ? "GDAL + POPPLER CACHE" : "NOT BUILT") << '\n'
                  << "  TERRAIN       " << (terrain_support_available() ? "GDAL GEOTIFF READY" : "GDAL NOT BUILT") << '\n'
                  << "  HERBS DB      " << (herbs_ready ? "READY / " + std::to_string(herb_stats.pages) + " PAGES" : "NOT AVAILABLE") << '\n'
                  << "  OLLAMA        " << (!ollama.enabled() ? "COMPILE-DISABLED" :
                      ready ? "REACHABLE" : "NOT RUNNING") << "  |  " << ollama.model() << '\n'
                  << "  NEXT BOOT EGG " << (std::filesystem::exists(boot_unlock_path()) ? "ARMED" : "NONE") << '\n';
        MeshProfile mesh;
        std::string mesh_error;
        const auto mesh_loaded = load_mesh_profile(mesh, mesh_error);
        const auto mesh_readiness = inspect_mesh_readiness();
        std::cout << "  FIELD I/O     UART SCOUT / :SCOUT  |  NEARBY MESH ";
        if (!mesh_loaded) std::cout << "PROFILE FAULT";
        else std::cout << mesh_power_mode_name(mesh.requested_mode) << " REQUESTED";
        std::cout << " / TX "
                  << (mesh_readiness.protocol_backend_ready ? "READY" : "LOCKED") << '\n';
        rule();
        const auto rows = vim_menu_rows(24);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER SELECT  |  Q BACK  |  TERMINAL MAY IGNORE ANSI PIXEL RESIZE\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("SYSTEM> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        const auto choice = vim_menu_choice(command, state, items.size());
        if (!choice) continue;
        if (*choice == 16) return;

        bool save = false;
        bool apply = false;
        if (*choice == 0) {
            profile.incident = choose_option(
                "ALTER SYSTEM INCIDENT MODE", incident_options(), ansi_enabled);
            std::string profile_error;
            if (!save_profile(profile, profile_error)) {
                paint(ansi_enabled, ansi::red);
                std::cout << "INCIDENT MODE NOT SAVED // " << profile_error << '\n';
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            }
            state = {};
            continue;
        } else if (*choice == 1) {
            settings.width_pixels = 1440;
            settings.height_pixels = 600;
            save = apply = true;
        } else if (*choice == 2) {
            settings.width_pixels = 1600;
            settings.height_pixels = 720;
            save = apply = true;
        } else if (*choice == 3) {
            settings.width_pixels = 1920;
            settings.height_pixels = 800;
            save = apply = true;
        } else if (*choice == 4) {
            const auto requested = lower(prompt("SIZE WIDTHXHEIGHT> ", ansi_enabled));
            const auto delimiter = requested.find('x');
            try {
                if (delimiter == std::string::npos) throw std::invalid_argument("missing x");
                const int width_pixels = std::stoi(requested.substr(0, delimiter));
                const int height_pixels = std::stoi(requested.substr(delimiter + 1));
                if (!valid_terminal_resolution(width_pixels, height_pixels)) {
                    throw std::out_of_range("resolution");
                }
                settings.width_pixels = width_pixels;
                settings.height_pixels = height_pixels;
                save = apply = true;
            } catch (...) {
                paint(ansi_enabled, ansi::red);
                std::cout << "INVALID SIZE // USE WIDTHXHEIGHT FROM 640X480 TO 3840X2160\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            }
        } else if (*choice == 5) {
            settings.resize_on_launch = !settings.resize_on_launch;
            save = true;
        } else if (*choice == 6) {
            apply = true;
        } else if (*choice == 7) {
            settings.theme = settings.theme == "blue" ? "gold" :
                settings.theme == "gold" ? "green" : "blue";
            apply_terminal_theme(settings.theme);
            save = true;
        } else if (*choice == 8) {
            const auto requested = upper(prompt("ROOM CODE [1-16 A-Z/0-9/-/_]> ", ansi_enabled));
            if (!valid_room_code(requested)) {
                paint(ansi_enabled, ansi::red);
                std::cout << "INVALID ROOM CODE // USE 1-16 LETTERS, DIGITS, '-' OR '_'\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            } else {
                settings.room_code = requested;
                save = true;
            }
        } else if (*choice == 9) {
            settings.layout_mode = settings.layout_mode == "workstation" ? "minimal" :
                settings.layout_mode == "minimal" ? "static" : "workstation";
            if (settings.layout_mode == "minimal") {
                settings.width_pixels = 800;
                settings.height_pixels = 600;
            } else {
                settings.width_pixels = 1440;
                settings.height_pixels = 600;
            }
            save = apply = true;
        } else if (*choice == 10) {
            settings.touch_keyboard = !settings.touch_keyboard;
            save = true;
        } else if (*choice == 11) {
            settings.companion_render = settings.companion_render == "auto" ? "ansi" :
                settings.companion_render == "ansi" ? "off" : "auto";
            save = true;
        } else if (*choice == 12) {
            settings.sentinel_movie_speed = next_sentinel_movie_speed(
                settings.sentinel_movie_speed);
            save = true;
        } else if (*choice == 13) {
            (void)edit_last_will_message(
                profile, ready, library.documents().size(), ansi_enabled);
            continue;
        } else if (*choice == 14) {
            if (!settings.sentinel_last_will_enabled && !will_ready) {
                paint(ansi_enabled, ansi::red);
                std::cout << "LAST WILL NOT ENABLED // WRITE AND SAVE A MESSAGE FIRST\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
                continue;
            }
            settings.sentinel_last_will_enabled =
                !settings.sentinel_last_will_enabled;
            save = true;
        } else if (*choice == 15) {
            sentinel_idle_screen(root, SentinelMode::Sentinel, ansi_enabled);
            continue;
        }

        std::string settings_error;
        if (save && !save_terminal_settings(settings, settings_error)) {
            paint(ansi_enabled, ansi::red);
            std::cout << settings_error << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        }
        if (apply) request_terminal_pixel_size(
            settings.width_pixels, settings.height_pixels, ansi_enabled);
    }
}

void herb_reader(
    const std::filesystem::path& database, const std::filesystem::path& root,
    const std::string& document_id, std::size_t page,
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    std::size_t line_offset = 0;
    bool previous_page_bottom = false;
    for (;;) {
        std::string error;
        const auto source = read_herb_page(database, root, document_id, page, error);
        shell_header(profile, ollama_ready, documents, ansi_enabled, "HERB SOURCE READER");
        if (!source) {
            paint(ansi_enabled, ansi::red);
            std::cout << error << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
            return;
        }
        page = source->page;
        const auto geometry = pager_geometry();
        const auto lines = pager_lines(source->text, geometry.columns);
        if (previous_page_bottom) {
            line_offset = pager_max_offset(lines, geometry.rows);
            previous_page_bottom = false;
        }
        paint(ansi_enabled, ansi::red);
        std::cout << "REFERENCE ONLY // A SOURCE PAGE IS NOT A TREATMENT OR PLANT IDENTIFICATION\n";
        paint(ansi_enabled, ansi::cyan);
        std::cout << upper(source->title) << '\n';
        paint(ansi_enabled, ansi::amber);
        std::cout << "PDF PAGE " << source->page << " / " << source->page_count << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        render_pager(lines, line_offset, geometry, ansi_enabled);
        const auto command = pager_prompt("HERB READER> ", ansi_enabled);
        const auto maximum = pager_max_offset(lines, geometry.rows);
        if (pager_scroll(command, line_offset, lines, geometry.rows)) continue;
        if (command == " " || command == "space" || command == "pgdn") {
            if (line_offset < maximum) line_offset = std::min(maximum, line_offset + geometry.rows);
            else if (page < source->page_count) { ++page; line_offset = 0; }
        } else if (command == "b" || command == "pgup") {
            if (line_offset > 0) {
                line_offset = line_offset > geometry.rows ? line_offset - geometry.rows : 0;
            } else if (page > 1) {
                --page;
                previous_page_bottom = true;
            }
        } else if ((command == "next" || command == "n") && page < source->page_count) {
            ++page;
            line_offset = 0;
        } else if ((command == "prev" || command == "p") && page > 1) {
            --page;
            line_offset = 0;
        }
        else if (command.rfind("goto ", 0) == 0) {
            try {
                page = std::max<std::size_t>(1, std::min(
                    static_cast<std::size_t>(std::stoull(command.substr(5))), source->page_count));
                line_offset = 0;
            } catch (...) {}
        } else if (command == "image" || command == "i") {
            pdf_image_viewer(
                source->pdf_path, source->document_id, source->title, page,
                source->page_count, root, profile, ollama_ready, documents, ansi_enabled,
                "REFERENCE IMAGE ONLY // NOT A TREATMENT OR CERTAIN PLANT IDENTIFICATION");
        } else if (command == "openpdf" || command == "o") {
#ifdef __APPLE__
            const std::string shell_command = "open \"" + source->pdf_path.string() + "\"";
            std::system(shell_command.c_str());
#endif
        } else if (command == "back" || command == "q" || command == "escape") return;
    }
}

void plant_image_cards_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const std::filesystem::path& root, const bool ansi_enabled) {
    struct PlantImageCard {
        std::string title;
        std::string subtitle;
        std::size_t page;
    };
    const std::vector<PlantImageCard> cards{
        {"POISON IVY", "EASTERN + WESTERN FORMS / LEAVES AND GROWTH HABIT", 2},
        {"POISON OAK", "SHRUB OR VINE / THREE OAK-LIKE LEAFLETS", 3},
        {"POISON SUMAC", "BOGGY HABITAT / 7-13 SMOOTH-EDGED LEAFLETS", 3},
    };
    const auto document = std::find_if(
        library.documents().begin(), library.documents().end(), [](const LibraryDocument& item) {
            return item.id == "usfs-poison-plants";
        });
    if (document == library.documents().end()) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "PLANT IMAGE CARDS");
        paint(ansi_enabled, ansi::red);
        std::cout << "USFS PLANT-IDENTIFICATION PDF IS NOT INSTALLED.\n";
        paint(ansi_enabled, ansi::muted);
        std::cout << "RUN scripts/download_fieldcraft_library.sh\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    std::vector<std::string> items;
    for (const auto& card : cards) items.push_back(card.title + "  //  " + card.subtitle);
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "PLANT IMAGE CARDS");
        paint(ansi_enabled, ansi::red);
        std::cout << "REFERENCE PHOTOS ONLY // DO NOT EAT OR HANDLE A PLANT BASED ON ONE IMAGE\n";
        paint(ansi_enabled, ansi::white);
        std::cout << "Confirm habitat, season, leaf arrangement, stems, fruit, and toxic lookalikes.\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows(18);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN ANSI PHOTO PAGE  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("PLANT CARD> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            pdf_image_viewer(
                document->pdf_path, document->id, cards[*choice].title,
                cards[*choice].page, document->pages, root, profile, ollama_ready,
                library.documents().size(), ansi_enabled,
                "REFERENCE PHOTO ONLY // MULTIPLE VIEWS OR EXPERT CONFIRMATION REQUIRED");
        }
    }
}

void herbs_screen(
    SurvivalLibrary& library, const OperatorProfile& profile, const bool ollama_ready,
    const std::filesystem::path& root, const bool ansi_enabled) {
    const auto documents = library.documents().size();
    const auto database = herb_database_path(root);
    HerbDatabaseStats stats;
    std::string error;
    if (!inspect_herb_database(database, stats, error)) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F7 / PLANTS + HERBS");
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::muted);
        std::cout << "BUILD COMMAND  scripts/download_plants_herbs_library.sh\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }

    std::string active_query;
    std::vector<HerbSearchHit> hits;
    VimMenuState state;
    const auto run_search = [&] {
        error.clear();
        hits = search_herb_database(database, root, active_query, 12, error);
        state = {};
    };
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F7 / PLANTS + HERBS");
        paint(ansi_enabled, ansi::red);
        std::cout << "SAFETY GATE // REFERENCE-ONLY EVIDENCE - NOT AUTOMATIC TREATMENT\n";
        paint(ansi_enabled, ansi::white);
        std::cout << "A text match does not establish plant identity, dose, efficacy, or safety.\n"
                     "Check toxic lookalikes, interactions, contraindications, contamination, and the exact PDF page.\n";
        paint(ansi_enabled, ansi::cyan);
        std::cout << "\nDATABASE  " << stats.documents << " PDF  |  " << stats.pages
                  << " PAGES  |  STRUCTURED PLANTS " << stats.structured_plants
                  << "  |  REVIEWED CLAIMS " << stats.reviewed_statements << '\n';
        paint(ansi_enabled, ansi::reset);

        std::vector<std::string> items;
        if (!active_query.empty()) {
            std::cout << "QUERY     " << active_query << '\n';
            if (hits.empty()) {
                paint(ansi_enabled, ansi::red);
                std::cout << "NO LOCAL PAGE MATCHES - TRY FEWER OR DIFFERENT TERMS.\n";
                paint(ansi_enabled, ansi::reset);
            }
            for (const auto& hit : hits) {
                items.push_back(hit.title + "  //  PDF PAGE " + std::to_string(hit.page));
            }
        }

        const auto rows = vim_menu_rows(active_query.empty() ? 18 : 24);
        render_vim_menu(items, state, rows, ansi_enabled);
        if (!hits.empty()) {
            paint(ansi_enabled, ansi::amber);
            std::cout << "SELECTED PASSAGE\n";
            paint(ansi_enabled, ansi::reset);
            const auto preview = pager_lines(hits[state.selected].excerpt, 96);
            const auto preview_rows = std::min<std::size_t>(3, preview.size());
            for (auto row = std::size_t{}; row < preview_rows; ++row) std::cout << preview[row] << '\n';
            for (auto row = preview_rows; row < 3; ++row) std::cout << '\n';
        }
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  S OR / SEARCH  |  I PLANT IMAGE CARDS";
        if (!hits.empty()) std::cout << "  |  ENTER/O OPEN";
        std::cout << "  |  C CLEAR  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("HERBS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") {
            if (!active_query.empty()) {
                active_query.clear();
                hits.clear();
                state = {};
                continue;
            }
            return;
        }
        if (command.empty() && hits.empty()) continue;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command == "image" || command == "images" || command == "i") {
            plant_image_cards_screen(library, profile, ollama_ready, root, ansi_enabled);
            continue;
        }
        if (command == "clear" || command == "c") {
            active_query.clear();
            hits.clear();
            state = {};
            continue;
        }
        if (command.rfind("search ", 0) == 0) active_query = command.substr(7);
        else if (command == "search" || command == "s" || command == "/") {
            active_query = prompt("HERB SEARCH> ", ansi_enabled);
        } else if (const auto choice = vim_menu_choice(command, state, hits.size())) {
            herb_reader(database, root, hits[*choice].document_id, hits[*choice].page,
                        profile, ollama_ready, documents, ansi_enabled);
            continue;
        } else {
            active_query = command;
        }
        run_search();
        if (!error.empty()) {
            paint(ansi_enabled, ansi::red);
            std::cout << error << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        }
    }
}

std::optional<double> coordinate_prompt(
    const std::string_view label, const double minimum, const double maximum,
    const bool ansi_enabled) {
    const auto value = prompt(std::string(label) + "> ", ansi_enabled);
    try {
        std::size_t consumed = 0;
        const auto parsed = std::stod(value, &consumed);
        if (consumed == value.size() && std::isfinite(parsed) &&
            parsed >= minimum && parsed <= maximum) return parsed;
    } catch (...) {}
    paint(ansi_enabled, ansi::red);
    std::cout << "INVALID COORDINATE // EXPECTED " << minimum << " TO " << maximum << '\n';
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
    return std::nullopt;
}

void map_marker_manager(
    MapAnnotationStore& store, MapAnnotations& annotations,
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F6 / MANUAL FIELD MARKERS");
        std::cout << "ID  KEY  LATITUDE     LONGITUDE      LABEL\n";
        rule();
        for (std::size_t index = 0; index < annotations.markers.size(); ++index) {
            const auto& marker = annotations.markers[index];
            std::cout << std::setw(2) << index + 1 << "   " << map_marker_symbol(marker.type)
                      << "   " << std::fixed << std::setprecision(6) << std::setw(11)
                      << marker.latitude << "  " << std::setw(12) << marker.longitude
                      << "  " << marker.name << " // " << upper(marker.type) << '\n';
        }
        if (annotations.markers.empty()) std::cout << "NO MANUAL FIELD MARKERS RECORDED.\n";
        rule();
        paint(ansi_enabled, ansi::muted);
        std::cout << "D <N> DELETE POINT  |  WAYPOINT ORDER IS INSERTION ORDER  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("FIELD MARKS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (command.rfind("d ", 0) == 0 || command.rfind("delete ", 0) == 0) {
            const auto number_text = command.substr(command.find(' ') + 1);
            try {
                const auto number = static_cast<std::size_t>(std::stoul(number_text));
                if (number == 0 || number > annotations.markers.size()) continue;
                const auto confirmation = lower(prompt("TYPE DELETE TO REMOVE POINT> ", ansi_enabled));
                if (confirmation != "delete") continue;
                annotations.markers.erase(annotations.markers.begin() + static_cast<std::ptrdiff_t>(number - 1));
                std::string error;
                if (!store.save(annotations, error)) {
                    paint(ansi_enabled, ansi::red);
                    std::cout << error << '\n';
                    paint(ansi_enabled, ansi::reset);
                    pause(ansi_enabled);
                }
            } catch (...) {}
        }
    }
}

bool add_map_marker(
    MapAnnotationStore& store, MapAnnotations& annotations, const TerrainMapInfo& full_bounds,
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    shell_header(profile, ollama_ready, documents, ansi_enabled, "F6 / ADD MANUAL MAP POINT");
    std::cout << std::fixed << std::setprecision(6)
              << "INSTALLED MAP BOUNDS  N " << full_bounds.north << "  S " << full_bounds.south
              << "  W " << full_bounds.west << "  E " << full_bounds.east << '\n';
    const auto latitude = coordinate_prompt("LATITUDE / DECIMAL DEGREES", -90.0, 90.0, ansi_enabled);
    if (!latitude) return false;
    const auto longitude = coordinate_prompt("LONGITUDE / DECIMAL DEGREES", -180.0, 180.0, ansi_enabled);
    if (!longitude) return false;
    if (*latitude < full_bounds.south || *latitude > full_bounds.north ||
        *longitude < full_bounds.west || *longitude > full_bounds.east) {
        paint(ansi_enabled, ansi::red);
        std::cout << "POINT IS OUTSIDE THIS INSTALLED STATE MAP. NOT SAVED.\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return false;
    }
    const auto name = prompt("LOCATION LABEL> ", ansi_enabled);
    if (name.empty()) return false;
    const auto& types = map_marker_types();
    std::cout << "KEY TYPE\n";
    for (std::size_t index = 0; index < types.size(); ++index) {
        std::cout << index + 1 << "  [" << map_marker_symbol(types[index]) << "] "
                  << upper(types[index]) << '\n';
    }
    const auto choice = prompt("TYPE [1-9]> ", ansi_enabled);
    std::size_t type_index = 0;
    try { type_index = static_cast<std::size_t>(std::stoul(choice)); } catch (...) { return false; }
    if (type_index == 0 || type_index > types.size()) return false;
    MapMarker marker;
    marker.id = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    marker.latitude = *latitude;
    marker.longitude = *longitude;
    marker.type = types[type_index - 1];
    marker.name = name;
    annotations.markers.push_back(std::move(marker));
    std::string error;
    if (store.save(annotations, error)) return true;
    annotations.markers.pop_back();
    paint(ansi_enabled, ansi::red);
    std::cout << error << '\n';
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
    return false;
}

std::string visible_landmark_report(const TerrainMapInfo& info) {
    std::ostringstream report;
    report << "VISIBLE OFFLINE LANDMARKS\n"
           << "===============================================================================\n"
           << "NO GPS REQUIRED. MATCH MAP SYMBOLS TO SIGNS, SKYLINE, WATER, ROAD, OR RAIL.\n\n"
           << "KEY  TYPE    MAP SECTOR  NAME\n"
           << "-------------------------------------------------------------------------------\n";
    const auto center_latitude = (info.north + info.south) / 2.0;
    const auto center_longitude = (info.west + info.east) / 2.0;
    for (const auto& landmark : info.visible_landmarks) {
        const std::string sector =
            landmark.latitude >= center_latitude
                ? (landmark.longitude < center_longitude ? "NW" : "NE")
                : (landmark.longitude < center_longitude ? "SW" : "SE");
        report << ' ' << landmark.symbol << "   " << std::left << std::setw(7)
               << landmark.kind << std::setw(12) << sector << landmark.name << '\n';
    }
    if (info.visible_landmarks.empty()) {
        report << "NO NAMED LANDMARK ICONS IN THIS VIEW. ZOOM OUT OR PAN.\n";
    }
    report << "\nSYMBOLS  @ TOWN  O LAKE  ~ RIVER  = NAMED ROAD  # RAIL TRACK  * TRAIL\n"
           << "CAUTION  A DRAWN ROUTE DOES NOT PROVE ACCESS, CONDITION, OR SAFETY.\n";
    return report.str();
}

void map_screen(
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const std::filesystem::path& root, const bool ansi_enabled) {
    const auto maps_root = root / "maps";
    const auto discovered_maps = discover_terrain_maps(maps_root);
    struct StateMapPack {
        std::string code;
        std::string name;
        std::vector<std::filesystem::path> maps;
        std::uintmax_t bytes{};
    };
    constexpr std::array<std::pair<std::string_view, std::string_view>, 50> state_names{{
        {"al", "Alabama"}, {"ak", "Alaska"}, {"az", "Arizona"}, {"ar", "Arkansas"},
        {"ca", "California"}, {"co", "Colorado"}, {"ct", "Connecticut"}, {"de", "Delaware"},
        {"fl", "Florida"}, {"ga", "Georgia"}, {"hi", "Hawaii"}, {"id", "Idaho"},
        {"il", "Illinois"}, {"in", "Indiana"}, {"ia", "Iowa"}, {"ks", "Kansas"},
        {"ky", "Kentucky"}, {"la", "Louisiana"}, {"me", "Maine"}, {"md", "Maryland"},
        {"ma", "Massachusetts"}, {"mi", "Michigan"}, {"mn", "Minnesota"},
        {"ms", "Mississippi"}, {"mo", "Missouri"}, {"mt", "Montana"}, {"ne", "Nebraska"},
        {"nv", "Nevada"}, {"nh", "New Hampshire"}, {"nj", "New Jersey"},
        {"nm", "New Mexico"}, {"ny", "New York"}, {"nc", "North Carolina"},
        {"nd", "North Dakota"}, {"oh", "Ohio"}, {"ok", "Oklahoma"}, {"or", "Oregon"},
        {"pa", "Pennsylvania"}, {"ri", "Rhode Island"}, {"sc", "South Carolina"},
        {"sd", "South Dakota"}, {"tn", "Tennessee"}, {"tx", "Texas"}, {"ut", "Utah"},
        {"vt", "Vermont"}, {"va", "Virginia"}, {"wa", "Washington"},
        {"wv", "West Virginia"}, {"wi", "Wisconsin"}, {"wy", "Wyoming"},
    }};
    const auto state_name = [&](const std::string& code) {
        const auto found = std::find_if(state_names.begin(), state_names.end(), [&](const auto& item) {
            return item.first == code;
        });
        return found == state_names.end() ? upper(code) : std::string(found->second);
    };
    std::vector<StateMapPack> state_packs;
    for (const auto& map : discovered_maps) {
        std::error_code relative_error;
        const auto relative = std::filesystem::relative(map, maps_root, relative_error);
        const auto component = relative.begin();
        const auto code = !relative_error && component != relative.end()
            ? lower(component->string()) : std::string("local");
        auto pack = std::find_if(state_packs.begin(), state_packs.end(), [&](const auto& item) {
            return item.code == code;
        });
        if (pack == state_packs.end()) {
            state_packs.push_back({code, state_name(code), {}, 0});
            pack = std::prev(state_packs.end());
        }
        pack->maps.push_back(map);
    }
    for (auto& pack : state_packs) {
        std::error_code size_error;
        const auto state_directory = maps_root / pack.code;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 state_directory, size_error)) {
            if (size_error) break;
            if (entry.is_regular_file(size_error)) pack.bytes += entry.file_size(size_error);
        }
    }
    std::sort(state_packs.begin(), state_packs.end(), [](const auto& left, const auto& right) {
        return left.name < right.name;
    });

    if (!terrain_support_available()) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F6 / OFFLINE MAPS");
        paint(ansi_enabled, ansi::red);
        std::cout << "GDAL TERRAIN SUPPORT IS NOT AVAILABLE IN THIS BUILD.\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    if (state_packs.empty()) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F6 / OFFLINE MAPS");
        std::cout << "NO STATE MAP PACKS FOUND UNDER " << maps_root << '\n';
        pause(ansi_enabled);
        return;
    }

    std::vector<std::string> state_items;
    for (const auto& pack : state_packs) {
        TerrainMapInfo info;
        std::string error;
        inspect_terrain(pack.maps.front(), info, error);
        std::ostringstream item;
        item << upper(pack.code) << " / " << upper(pack.name) << "  //  READY  //  "
             << pack.maps.size() << " TERRAIN  //  " << std::fixed << std::setprecision(1)
             << static_cast<double>(pack.bytes) / (1024.0 * 1024.0) << " MB  //  "
             << info.trail_feature_count << " TRAILS";
        state_items.push_back(item.str());
    }
    VimMenuState state_selector;
    std::size_t selected_state = 0;
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F6 / STATE MAP PACKS");
        paint(ansi_enabled, ansi::amber_bar);
        std::cout << " SELECT INSTALLED STATE // PACKS LOAD INDEPENDENTLY ";
        paint(ansi_enabled, ansi::reset);
        std::cout << '\n';
        rule();
        const auto rows = vim_menu_rows(17);
        render_vim_menu(state_items, state_selector, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN STATE  |  Q BACK  |  ONLY INSTALLED PACKS USE STORAGE\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("STATE MAP> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state_selector, state_items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state_selector, state_items.size())) {
            selected_state = *choice;
            break;
        }
    }

    const auto& maps = state_packs[selected_state].maps;
    MapAnnotationStore annotation_store(
        root / "state" / "map-overlays" / (state_packs[selected_state].code + ".tsv"));
    MapAnnotations annotations;
    std::string annotation_error;
    annotation_store.load(annotations, annotation_error);
    TerrainMapInfo full_bounds;
    std::string bounds_error;
    inspect_terrain(maps.front(), full_bounds, bounds_error);
    std::vector<std::string> items;
    for (const auto& map : maps) {
        TerrainMapInfo info;
        std::string error;
        inspect_terrain(map, info, error);
        std::ostringstream item;
        item << std::left << std::setw(58) << map.filename().string() << std::right
             << std::setw(5) << info.source_width << 'x' << info.source_height << "  "
             << std::fixed << std::setprecision(0) << info.minimum_elevation_m << ".."
             << info.maximum_elevation_m << " M";
        items.push_back(item.str());
    }
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            "F6 / " + upper(state_packs[selected_state].code) + " OFFLINE MAPS");
        const auto rows = vim_menu_rows();
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  ENTER/O OPEN  |  Q COMMAND CENTER  |  :OPEN <N>\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("MAP> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, items.size())) {
            const auto index = *choice + 1;
            TerrainViewport viewport{0.5, 0.5, 4.0};
            for (;;) {
                const auto terminal = terminal_size();
                const auto map_columns = std::clamp<std::size_t>(
                    terminal.columns > 2 ? terminal.columns - 2 : terminal.columns, 40, 96);
                const auto map_reserved_rows = compact_display() ? std::size_t{9} : std::size_t{17};
                const auto map_rows = std::clamp<std::size_t>(
                    terminal.rows > map_reserved_rows ? terminal.rows - map_reserved_rows : 8,
                    8, 28);
                TerrainMapInfo info;
                std::string error;
                std::ostringstream rendered;
                if (!render_terrain_ansi(
                        maps[index - 1], map_columns, map_rows, ansi_enabled,
                        rendered, info, error, viewport, &annotations)) {
                    map_view_header(
                        profile, ansi_enabled, maps[index - 1].filename().string());
                    paint(ansi_enabled, ansi::red);
                    std::cout << error << '\n';
                    paint(ansi_enabled, ansi::reset);
                    pause(ansi_enabled);
                    break;
                }

                map_view_header(
                    profile, ansi_enabled, maps[index - 1].filename().string());
                if (compact_display()) {
                    paint(ansi_enabled, ansi::muted);
                    std::cout << std::fixed << std::setprecision(3)
                              << "N" << info.north << " S" << info.south
                              << " W" << info.west << " E" << info.east << '\n';
                    paint(ansi_enabled, ansi::cyan);
                    std::cout << "KEY @TOWN OLAKE ~RIVER =ROAD #RAIL *TRAIL | N NAMES\n";
                    paint(ansi_enabled, ansi::green);
                    std::cout << "VISIBLE " << info.visible_landmarks.size()
                              << " LANDMARKS | OFFLINE ORIENTATION | NO GPS REQUIRED\n";
                } else {
                    paint(ansi_enabled, ansi::muted);
                    std::cout << std::fixed << std::setprecision(4)
                              << "N " << info.north << "  S " << info.south
                              << "  W " << info.west << "  E " << info.east << '\n'
                              << std::setprecision(0) << "ELEVATION " << info.minimum_elevation_m
                              << " TO " << info.maximum_elevation_m << " METERS  |  "
                              << info.projection << '\n';
                    paint(ansi_enabled, ansi::cyan);
                    std::cout << "LANDMARKS " << info.visible_landmarks.size()
                              << " VISIBLE | " << info.town_feature_count << " TOWNS | "
                              << info.water_feature_count << " WATERS | "
                              << info.river_feature_count << " RIVERS | "
                              << info.road_feature_count << " ROADS | "
                              << info.rail_feature_count << " RAIL | "
                              << info.trail_feature_count << " TRAILS\n";
                    paint(ansi_enabled, ansi::green);
                    std::cout << "USER OVERLAY " << annotations.markers.size() << " POINTS  |  WAYPOINTS "
                              << (annotations.connect_waypoints ? "ON" : "OFF") << "  |  ROUTE "
                              << std::fixed << std::setprecision(2)
                              << waypoint_distance_miles(annotations.markers)
                              << " MI  |  GREEN KEYS / ORANGE +\n";
                }
                paint(ansi_enabled, ansi::reset);
                rule();
                std::cout << rendered.str();
                rule();
                paint(ansi_enabled, ansi::cyan);
                std::cout << "VIEW  " << std::setprecision(1) << viewport.zoom
                          << "X  |  CENTER " << std::setprecision(4)
                          << (info.north + info.south) / 2.0 << " N  "
                          << (info.west + info.east) / 2.0 << " E\n";
                paint(ansi_enabled, ansi::muted);
                if (compact_display()) {
                    std::cout << "ARROWS PAN | +/- ZOOM | R CENTER | N NAMES | M ADD | P MARKS | ESC\n";
                } else {
                    std::cout << "PAN ARROWS/HJKL | +/- ZOOM | R CENTER | N LANDMARKS | M ADD | P MARKS | W ROUTE | Q LIST\n";
                }
                paint(ansi_enabled, ansi::reset);

                const auto navigation = pager_prompt("TERRAIN> ", ansi_enabled);
                const auto half_window = 0.5 / viewport.zoom;
                const auto pan_step = 0.2 / viewport.zoom;
                if (navigation == "left" || navigation == "h") {
                    viewport.center_x = std::max(half_window, viewport.center_x - pan_step);
                } else if (navigation == "right" || navigation == "l") {
                    viewport.center_x = std::min(1.0 - half_window, viewport.center_x + pan_step);
                } else if (navigation == "up" || navigation == "k") {
                    viewport.center_y = std::max(half_window, viewport.center_y - pan_step);
                } else if (navigation == "down" || navigation == "j") {
                    viewport.center_y = std::min(1.0 - half_window, viewport.center_y + pan_step);
                } else if (navigation == "+" || navigation == "=") {
                    viewport.zoom = std::min(32.0, viewport.zoom * 2.0);
                } else if (navigation == "-" || navigation == "_") {
                    viewport.zoom = std::max(1.0, viewport.zoom / 2.0);
                    const auto new_half = 0.5 / viewport.zoom;
                    viewport.center_x = std::clamp(viewport.center_x, new_half, 1.0 - new_half);
                    viewport.center_y = std::clamp(viewport.center_y, new_half, 1.0 - new_half);
                } else if (navigation == "r" || navigation == "recenter") {
                    viewport.center_x = 0.5;
                    viewport.center_y = 0.5;
                } else if (navigation == "n" || navigation == "names" ||
                           navigation == "landmarks") {
                    const auto report = visible_landmark_report(info);
                    vim_text_screen(
                        profile, ollama_ready, documents, ansi_enabled,
                        "F6 / VISIBLE LANDMARKS", "OFFLINE ORIENTATION INDEX", report);
                } else if (navigation == "m" || navigation == "mark" ||
                           navigation == "g" || navigation == "gps") {
                    add_map_marker(
                        annotation_store, annotations, full_bounds, profile,
                        ollama_ready, documents, ansi_enabled);
                } else if (navigation == "p" || navigation == "points") {
                    map_marker_manager(
                        annotation_store, annotations, profile, ollama_ready,
                        documents, ansi_enabled);
                } else if (navigation == "w" || navigation == "waypoints") {
                    annotations.connect_waypoints = !annotations.connect_waypoints;
                    std::string save_error;
                    if (!annotation_store.save(annotations, save_error)) {
                        paint(ansi_enabled, ansi::red);
                        std::cout << save_error << '\n';
                        paint(ansi_enabled, ansi::reset);
                        pause(ansi_enabled);
                    }
                } else if (navigation == "q" || navigation == "back" ||
                           navigation == "escape") {
                    break;
                }
            }
        }
    }
}

std::string journal_kind_label(std::string value) {
    std::replace(value.begin(), value.end(), '_', ' ');
    return value;
}

std::optional<std::string> journal_composer(
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled, std::string_view title, std::string_view initial_body,
    const std::string_view section) {
    std::vector<std::string> lines;
    if (!initial_body.empty()) {
        std::istringstream input{std::string(initial_body)};
        std::string line;
        while (std::getline(input, line)) lines.push_back(line);
    }
    for (;;) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, section);
        paint(ansi_enabled, ansi::cyan);
        std::cout << upper(std::string(title)) << '\n';
        paint(ansi_enabled, ansi::white);
        std::cout << "WRITE MODE // LOCAL PLAINTEXT // " << lines.size() << " LINES\n";
        paint(ansi_enabled, ansi::reset);
        rule();

        const auto terminal = terminal_size();
        const auto visible_rows = std::clamp<std::size_t>(
            terminal.rows > 17 ? terminal.rows - 17 : 6, 6, 25);
        const auto start = lines.size() > visible_rows ? lines.size() - visible_rows : 0;
        const auto columns = std::max<std::size_t>(30, std::min(width, terminal.columns));
        for (auto index = start; index < lines.size(); ++index) {
            auto visible = lines[index];
            const auto available = columns > 8 ? columns - 8 : columns;
            if (visible.size() > available) visible = visible.substr(0, available - 3) + "...";
            paint(ansi_enabled, ansi::muted);
            std::cout << std::setw(4) << index + 1 << "  ";
            paint(ansi_enabled, ansi::white);
            std::cout << visible << '\n';
        }
        for (auto row = lines.size() - start; row < visible_rows; ++row) std::cout << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        paint(ansi_enabled, ansi::cyan);
        std::cout << "WRITE LOCK  |  .SAVE COMMIT  .UNDO LAST  .CLEAR ALL  .CANCEL ABORT\n";
        paint(ansi_enabled, ansi::muted);
        std::cout << "A LINE BEGINNING WITH A DOT CAN BE WRITTEN BY PREFIXING A SECOND DOT.\n";
        paint(ansi_enabled, ansi::reset);
        auto line = prompt("| ", ansi_enabled);
        const auto command = lower(line);
        if (command == ".cancel") return std::nullopt;
        if (command == ".undo") {
            if (!lines.empty()) lines.pop_back();
            continue;
        }
        if (command == ".clear") {
            const auto confirmation = lower(prompt("TYPE CLEAR TO REMOVE COMPOSER LINES> ", ansi_enabled));
            if (confirmation == "clear") lines.clear();
            continue;
        }
        if (command == ".save") {
            const bool has_text = std::any_of(lines.begin(), lines.end(), [](const std::string& item) {
                return std::any_of(item.begin(), item.end(), [](const unsigned char character) {
                    return !std::isspace(character);
                });
            });
            if (!has_text) {
                paint(ansi_enabled, ansi::red);
                std::cout << "EMPTY ENTRY NOT SAVED.\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
                continue;
            }
            std::ostringstream body;
            for (auto index = std::size_t{}; index < lines.size(); ++index) {
                if (index > 0) body << '\n';
                body << lines[index];
            }
            return body.str();
        }
        if (line.rfind("..", 0) == 0) line.erase(0, 1);
        lines.push_back(std::move(line));
    }
}

bool edit_last_will_message(
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    std::string error;
    const auto existing = load_sentinel_last_will(sentinel_last_will_path(), error);
    if (!error.empty()) {
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return false;
    }
    const auto message = journal_composer(
        profile, ollama_ready, documents, ansi_enabled,
        "LAST WILL & TESTAMENT // IF THIS WAYKEEPER IS FOUND",
        existing.value_or(std::string{}), "F5 / LAST WILL FOUND-DEVICE MESSAGE");
    if (!message) return false;
    if (!save_sentinel_last_will(sentinel_last_will_path(), *message, error)) {
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return false;
    }
    shell_header(profile, ollama_ready, documents, ansi_enabled, "F5 / LAST WILL SAVED");
    paint(ansi_enabled, ansi::green);
    std::cout << "FOUND-DEVICE MESSAGE SAVED LOCALLY // DISPLAY TOGGLE UNCHANGED\n";
    paint(ansi_enabled, ansi::muted);
    std::cout << "This feature publishes text when enabled; it does not execute or verify a legal will.\n";
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
    return true;
}

bool create_journal_entry(
    JournalStore& store, const std::string& kind, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents, const bool ansi_enabled) {
    shell_header(profile, ollama_ready, documents, ansi_enabled, "F8 / NEW JOURNAL ENTRY");
    paint(ansi_enabled, ansi::cyan);
    std::cout << journal_kind_label(kind) << '\n';
    paint(ansi_enabled, ansi::reset);
    const auto default_title = kind == "CAPTAINS_LOG" ? "Captain's Log // " + timestamp() :
                                                        "Survival Help Note // " + timestamp();
    auto title = prompt("TITLE [RETURN FOR DEFAULT]> ", ansi_enabled);
    if (title.empty()) title = default_title;
    const auto tags = prompt("TAGS [COMMA SEPARATED / OPTIONAL]> ", ansi_enabled);
    std::string initial_body;
    double sleep_hours = 0.0;
    double miles_traveled = 0.0;
    std::string health_note;
    if (kind == "CAPTAINS_LOG") {
        const auto position = prompt("POSITION / LOCATION [OPTIONAL]> ", ansi_enabled);
        const auto conditions = prompt("WEATHER / CONDITIONS [OPTIONAL]> ", ansi_enabled);
        const auto watch = prompt("WATCH / SHIFT [OPTIONAL]> ", ansi_enabled);
        if (const auto value = inventory_number_prompt("SLEEP HOURS / LAST 24H", 0.0, ansi_enabled)) {
            sleep_hours = std::min(24.0, *value);
        }
        if (const auto value = inventory_number_prompt("MILES TRAVELED / TODAY", 0.0, ansi_enabled)) {
            miles_traveled = *value;
        }
        health_note = prompt("HEALTH / CONDITION NOTE [OPTIONAL]> ", ansi_enabled);
        if (!position.empty()) initial_body += "POSITION / LOCATION: " + position;
        if (!conditions.empty()) {
            if (!initial_body.empty()) initial_body += '\n';
            initial_body += "WEATHER / CONDITIONS: " + conditions;
        }
        if (!watch.empty()) {
            if (!initial_body.empty()) initial_body += '\n';
            initial_body += "WATCH / SHIFT: " + watch;
        }
    }
    const auto body = journal_composer(
        profile, ollama_ready, documents, ansi_enabled, title, initial_body);
    if (!body) return false;

    JournalEntry entry;
    entry.kind = kind;
    entry.title = title;
    entry.tags = tags;
    entry.operator_name = profile.name;
    entry.incident = profile.incident;
    entry.terrain = profile.terrain;
    entry.sleep_hours = sleep_hours;
    entry.miles_traveled = miles_traveled;
    entry.health_note = health_note;
    entry.body = *body;
    std::string error;
    if (!store.create(entry, error)) {
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F8 / JOURNAL FAULT");
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return false;
    }
    return true;
}

bool edit_journal_entry(
    JournalStore& store, JournalEntry& entry, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents, const bool ansi_enabled) {
    shell_header(profile, ollama_ready, documents, ansi_enabled, "F8 / EDIT JOURNAL ENTRY");
    std::cout << "CURRENT TITLE  " << entry.title << '\n'
              << "CURRENT TAGS   " << (entry.tags.empty() ? "-" : entry.tags) << '\n';
    auto title = prompt("NEW TITLE [RETURN KEEPS CURRENT]> ", ansi_enabled);
    if (!title.empty()) entry.title = title;
    auto tags = prompt("NEW TAGS [RETURN KEEPS CURRENT / - CLEARS]> ", ansi_enabled);
    if (tags == "-") entry.tags.clear();
    else if (!tags.empty()) entry.tags = tags;
    if (entry.kind == "CAPTAINS_LOG") {
        if (const auto value = inventory_number_prompt(
                "SLEEP HOURS / LAST 24H", entry.sleep_hours, ansi_enabled)) {
            entry.sleep_hours = std::min(24.0, *value);
        }
        if (const auto value = inventory_number_prompt(
                "MILES TRAVELED / TODAY", entry.miles_traveled, ansi_enabled)) {
            entry.miles_traveled = *value;
        }
        const auto health = prompt("HEALTH NOTE [RETURN KEEPS / - CLEARS]> ", ansi_enabled);
        if (health == "-") entry.health_note.clear();
        else if (!health.empty()) entry.health_note = health;
    }
    const auto replace = lower(prompt("REPLACE BODY IN WRITE LOCK? [Y/N]> ", ansi_enabled));
    if (replace == "y" || replace == "yes") {
        const auto body = journal_composer(
            profile, ollama_ready, documents, ansi_enabled, entry.title, entry.body);
        if (!body) return false;
        entry.body = *body;
    }
    std::string error;
    if (!store.update(entry, error)) {
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return false;
    }
    return true;
}

bool journal_entry_screen(
    JournalStore& store, JournalEntry entry, const bool archived,
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    std::size_t offset = 0;
    for (;;) {
        const auto geometry = pager_geometry();
        const auto lines = pager_lines(entry.body, geometry.columns);
        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            archived ? "F8 / ARCHIVED LOG ENTRY" : "F8 / JOURNAL ENTRY");
        paint(ansi_enabled, ansi::cyan);
        std::cout << upper(entry.title) << '\n';
        paint(ansi_enabled, ansi::amber);
        std::cout << journal_kind_label(entry.kind) << "  |  CREATED " << entry.created_at
                  << "  |  UPDATED " << entry.updated_at << '\n';
        paint(ansi_enabled, ansi::muted);
        std::cout << "OPERATOR " << entry.operator_name << "  |  INCIDENT " << entry.incident
                  << "  |  TERRAIN " << entry.terrain << '\n'
                  << "TAGS " << (entry.tags.empty() ? "-" : entry.tags) << "  |  ID " << entry.id << '\n';
        if (entry.kind == "CAPTAINS_LOG") {
            std::cout << "HEALTH LOG  SLEEP " << inventory_number(entry.sleep_hours)
                      << " H  |  TRAVELED " << inventory_number(entry.miles_traveled)
                      << " MI  |  " << (entry.health_note.empty() ? "NO CONDITION NOTE" : entry.health_note)
                      << '\n';
        }
        paint(ansi_enabled, ansi::reset);
        rule();
        render_vim_text(lines, offset, geometry, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << (archived ? "ACTION  R RESTORE  |  Q BACK\n" :
                                  "ACTION  E EDIT  |  A ARCHIVE  |  Q BACK\n");
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("LOG ENTRY> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return false;
        if (pager_scroll(command, offset, lines, geometry.rows)) continue;
        const auto maximum = pager_max_offset(lines, geometry.rows);
        if (command == " " || command == "space" || command == "pgdn") {
            offset = std::min(maximum, offset + geometry.rows);
        } else if (command == "b" || command == "pgup") {
            offset = offset > geometry.rows ? offset - geometry.rows : 0;
        } else if (!archived && (command == "edit" || command == "e")) {
            if (edit_journal_entry(
                    store, entry, profile, ollama_ready, documents, ansi_enabled)) {
                offset = 0;
                return true;
            }
        } else if (!archived && (command == "archive" || command == "a")) {
            const auto confirmation = lower(prompt("TYPE ARCHIVE TO MOVE THIS ENTRY> ", ansi_enabled));
            if (confirmation != "archive") continue;
            std::string error;
            if (store.archive(entry.id, error)) return true;
            paint(ansi_enabled, ansi::red);
            std::cout << error << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        } else if (archived && (command == "restore" || command == "r")) {
            const auto confirmation = lower(prompt("TYPE RESTORE TO RETURN THIS ENTRY> ", ansi_enabled));
            if (confirmation != "restore") continue;
            std::string error;
            if (store.restore(entry.id, error)) return true;
            paint(ansi_enabled, ansi::red);
            std::cout << error << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        }
    }
}

std::string citizen_field(const JournalEntry& entry, const std::string_view field) {
    const std::string prefix = std::string(field) + ":";
    std::istringstream input(entry.body);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind(prefix, 0) != 0) continue;
        auto value = line.substr(prefix.size());
        const auto first = value.find_first_not_of(' ');
        return first == std::string::npos ? std::string{} : value.substr(first);
    }
    return {};
}

bool create_citizen_record(
    JournalStore& store, const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    shell_header(profile, ollama_ready, documents, ansi_enabled, "F8 / CITIZENS / NEW RESIDENT");
    paint(ansi_enabled, ansi::cyan_bar);
    std::cout << " PRIVATE LOCAL RESIDENT + MEDICAL-STYLE REGISTRY ";
    paint(ansi_enabled, ansi::reset);
    std::cout << '\n';
    paint(ansi_enabled, ansi::red);
    std::cout << "SELF-REPORTED FIELD RECORD // NOT A DIAGNOSIS OR SUBSTITUTE FOR CLINICAL CARE\n";
    paint(ansi_enabled, ansi::reset);
    rule();

    const auto name = prompt("FULL / PREFERRED NAME> ", ansi_enabled);
    if (name.empty()) return false;
    const auto call_sign = prompt("CALL SIGN / ALIAS [OPTIONAL]> ", ansi_enabled);
    const auto age = prompt("AGE OR DOB [OPTIONAL]> ", ansi_enabled);
    const auto pronouns = prompt("PRONOUNS [OPTIONAL]> ", ansi_enabled);
    auto status = upper(prompt("STATUS [RESIDENT]> ", ansi_enabled));
    if (status.empty()) status = "RESIDENT";
    const auto arrival = prompt("ARRIVAL DATE [OPTIONAL]> ", ansi_enabled);
    const auto housing = prompt("HOUSING / BERTH [UNASSIGNED]> ", ansi_enabled);
    const auto blood = upper(prompt("BLOOD TYPE [UNKNOWN / SELF-REPORTED]> ", ansi_enabled));
    const auto allergies = prompt("ALLERGIES [UNKNOWN / NONE / LIST]> ", ansi_enabled);
    const auto medications = prompt("MEDICATIONS [UNKNOWN / NONE / LIST]> ", ansi_enabled);
    const auto conditions = prompt("MEDICAL CONDITIONS [UNKNOWN / NONE / LIST]> ", ansi_enabled);
    const auto access = prompt("MOBILITY / ACCESS NEEDS [OPTIONAL]> ", ansi_enabled);
    const auto skills = prompt("SKILLS / CAMP DUTIES [OPTIONAL]> ", ansi_enabled);
    const auto kin = prompt("NEXT OF KIN / EMERGENCY CONTACT [OPTIONAL]> ", ansi_enabled);

    const auto known_or = [](const std::string& value, const std::string_view fallback) {
        return value.empty() ? std::string(fallback) : value;
    };
    std::ostringstream initial;
    initial << "CITIZEN / RESIDENT: " << name << '\n'
            << "CALL SIGN / ALIAS: " << known_or(call_sign, "-") << '\n'
            << "AGE / DOB: " << known_or(age, "UNKNOWN") << '\n'
            << "PRONOUNS: " << known_or(pronouns, "-") << '\n'
            << "STATUS: " << status << '\n'
            << "ARRIVAL DATE: " << known_or(arrival, "UNKNOWN") << '\n'
            << "HOUSING / BERTH: " << known_or(housing, "UNASSIGNED") << '\n'
            << "BLOOD TYPE: " << known_or(blood, "UNKNOWN / SELF-REPORTED") << '\n'
            << "ALLERGIES: " << known_or(allergies, "UNKNOWN") << '\n'
            << "MEDICATIONS: " << known_or(medications, "UNKNOWN") << '\n'
            << "MEDICAL CONDITIONS: " << known_or(conditions, "UNKNOWN") << '\n'
            << "MOBILITY / ACCESS NEEDS: " << known_or(access, "-") << '\n'
            << "SKILLS / CAMP DUTIES: " << known_or(skills, "-") << '\n'
            << "NEXT OF KIN / EMERGENCY CONTACT: " << known_or(kin, "-") << '\n'
            << "MEDICAL NOTICE: SELF-REPORTED FIELD DATA / VERIFY WHEN PROFESSIONAL CARE IS AVAILABLE\n"
            << "\nNOTES / OBSERVATIONS:";
    const auto body = journal_composer(
        profile, ollama_ready, documents, ansi_enabled,
        "Citizen // " + name, initial.str());
    if (!body) return false;

    JournalEntry entry;
    entry.kind = "CITIZEN";
    entry.title = name;
    entry.tags = "citizen,resident," + lower(status);
    entry.operator_name = profile.name;
    entry.incident = profile.incident;
    entry.terrain = profile.terrain;
    entry.body = *body;
    std::string error;
    if (store.create(entry, error)) return true;
    paint(ansi_enabled, ansi::red);
    std::cout << "CITIZEN RECORD NOT SAVED // " << error << '\n';
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
    return false;
}

void citizen_registry_screen(
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    JournalStore store(journal_path());
    std::string active_query;
    VimMenuState state;
    for (;;) {
        std::string error;
        auto residents = active_query.empty()
            ? store.entries(error) : store.search(active_query, error);
        std::erase_if(residents, [](const auto& entry) { return entry.kind != "CITIZEN"; });
        std::vector<std::string> items;
        items.reserve(residents.size());
        for (const auto& resident : residents) {
            const auto status = citizen_field(resident, "STATUS");
            const auto blood = citizen_field(resident, "BLOOD TYPE");
            const auto berth = citizen_field(resident, "HOUSING / BERTH");
            std::ostringstream item;
            item << std::left << std::setw(27) << resident.title
                 << " | " << std::setw(10) << (status.empty() ? "UNKNOWN" : status)
                 << " | BLOOD " << std::setw(9) << (blood.empty() ? "UNKNOWN" : blood)
                 << " | " << (berth.empty() ? "UNASSIGNED" : berth);
            items.push_back(item.str());
        }
        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            "F8 / CITIZENS // RESIDENT MANIFEST + MEDICAL REGISTRY");
        paint(ansi_enabled, ansi::cyan_bar);
        std::cout << " CITIZENS // PRIVATE LOCAL CAMP REGISTRY ";
        paint(ansi_enabled, ansi::reset);
        std::cout << "  RESIDENTS " << residents.size() << '\n';
        paint(ansi_enabled, ansi::red);
        std::cout << "SELF-REPORTED MEDICAL-STYLE RECORDS // NOT DIAGNOSIS // PROTECT THIS DATA\n";
        paint(ansi_enabled, ansi::reset);
        if (!active_query.empty()) std::cout << "FILTER " << active_query << '\n';
        rule();
        const auto rows = vim_menu_rows(active_query.empty() ? 17 : 18);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ACTION  N NEW CITIZEN  |  ENTER/O OPEN RECORD  |  S SEARCH";
        if (!active_query.empty()) std::cout << "  |  C CLEAR";
        std::cout << "  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("CITIZENS> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (command == "new" || command == "n" || command == "add") {
            create_citizen_record(store, profile, ollama_ready, documents, ansi_enabled);
            state = {};
            continue;
        }
        if (command == "search" || command == "s" || command == "/") {
            active_query = prompt("CITIZEN SEARCH> ", ansi_enabled);
            state = {};
            continue;
        }
        if (command == "clear" || command == "c") {
            active_query.clear();
            state = {};
            continue;
        }
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (const auto choice = vim_menu_choice(command, state, residents.size())) {
            journal_entry_screen(
                store, residents[*choice], false, profile,
                ollama_ready, documents, ansi_enabled);
            state = {};
        }
    }
}

void journal_health_log_screen(
    JournalStore& store, const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    std::string error;
    auto entries = store.entries(error);
    std::erase_if(entries, [](const auto& entry) { return entry.kind != "CAPTAINS_LOG"; });
    double sleep_total = 0.0;
    double miles_total = 0.0;
    std::vector<std::string> rows;
    for (const auto& entry : entries) {
        sleep_total += entry.sleep_hours;
        miles_total += entry.miles_traveled;
        const auto date = entry.created_at.size() >= 10 ? entry.created_at.substr(0, 10) : entry.created_at;
        std::ostringstream row;
        row << date << "  SLEEP " << std::fixed << std::setprecision(1) << entry.sleep_hours
            << " H  |  TRAVEL " << entry.miles_traveled << " MI  |  "
            << (entry.health_note.empty() ? "NO CONDITION NOTE" : entry.health_note);
        rows.push_back(row.str());
    }
    std::size_t offset = 0;
    for (;;) {
        const auto geometry = pager_geometry();
        shell_header(profile, ollama_ready, documents, ansi_enabled, "F8 / HEALTH + TRAVEL LOG");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "CAPTAIN'S LOG METRICS  |  " << entries.size() << " DAYS  |  "
                  << std::fixed << std::setprecision(1) << sleep_total << " SLEEP HOURS  |  "
                  << miles_total << " MILES\n";
        paint(ansi_enabled, ansi::muted);
        if (!entries.empty()) {
            std::cout << "AVERAGE SLEEP " << sleep_total / static_cast<double>(entries.size())
                      << " H / LOGGED DAY\n";
        }
        if (!error.empty()) std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        rule();
        render_vim_text(rows, offset, geometry, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "SCROLL J/K SPACE/B  |  EACH ROW IS STORED WITH ITS CAPTAIN'S LOG  |  Q BACK\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("HEALTH LOG> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        pager_scroll(command, offset, rows, geometry.rows);
    }
}

void journal_screen(
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    JournalStore store(journal_path());
    std::string active_query;
    bool archived = false;
    VimMenuState state;
    for (;;) {
        std::string error;
        auto entries = archived ? store.archived_entries(error) :
            (active_query.empty() ? store.entries(error) : store.search(active_query, error));
        std::erase_if(entries, [](const auto& entry) { return entry.kind == "CITIZEN"; });
        std::vector<std::string> items;
        items.reserve(entries.size());
        for (const auto& entry : entries) {
            const auto date = entry.updated_at.size() >= 16 ? entry.updated_at.substr(0, 16) : entry.updated_at;
            std::ostringstream item;
            item << date << "  " << std::left << std::setw(15)
                 << journal_kind_label(entry.kind) << "  " << entry.title;
            if (!entry.tags.empty()) item << "  # " << entry.tags;
            items.push_back(item.str());
        }

        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            archived ? "F8 / JOURNAL ARCHIVE" : "F8 / CAPTAIN'S LOG + SURVIVAL NOTES");
        paint(ansi_enabled, ansi::cyan);
        std::cout << (archived ? "ARCHIVED ENTRIES" : "LOCAL JOURNAL") << "  |  "
                  << entries.size() << (entries.size() == 1 ? " ENTRY" : " ENTRIES") << '\n';
        paint(ansi_enabled, ansi::muted);
        std::cout << "STORE  " << store.root() << '\n';
        if (!active_query.empty()) std::cout << "FILTER " << active_query << '\n';
        if (!error.empty()) {
            paint(ansi_enabled, ansi::red);
            std::cout << error << '\n';
        }
        paint(ansi_enabled, ansi::reset);
        rule();
        const auto rows = vim_menu_rows(active_query.empty() ? 18 : 19);
        render_vim_menu(items, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        if (archived) {
            std::cout << "ACTION  ENTER/O OPEN  |  A ACTIVE LOG  |  Q BACK\n";
        } else {
            std::cout << "ACTION  L CAPTAIN'S LOG  |  H HEALTH LOG  |  N NOTE  |  C CITIZENS  |  S SEARCH  |  A ARCHIVE";
            if (!active_query.empty()) std::cout << "  |  C CLEAR";
            std::cout << "  |  Q BACK\n";
        }
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("JOURNAL> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") {
            if (archived) {
                archived = false;
                state = {};
                continue;
            }
            if (!active_query.empty()) {
                active_query.clear();
                state = {};
                continue;
            }
            return;
        }
        if (navigate_vim_menu(command, state, items.size(), rows)) continue;
        if (command == "archive" || command == "archived" || command == "a") {
            archived = !archived;
            active_query.clear();
            state = {};
            continue;
        }
        if (!archived && (command == "log" || command == "l")) {
            create_journal_entry(
                store, "CAPTAINS_LOG", profile, ollama_ready, documents, ansi_enabled);
            state = {};
            continue;
        }
        if (!archived && (command == "health" || command == "h")) {
            journal_health_log_screen(
                store, profile, ollama_ready, documents, ansi_enabled);
            state = {};
            continue;
        }
        if (!archived && (command == "note" || command == "new" || command == "n")) {
            create_journal_entry(
                store, "SURVIVAL_NOTE", profile, ollama_ready, documents, ansi_enabled);
            state = {};
            continue;
        }
        if (!archived && active_query.empty() &&
            (command == "citizens" || command == "citizen" || command == "c")) {
            citizen_registry_screen(profile, ollama_ready, documents, ansi_enabled);
            state = {};
            continue;
        }
        if (!archived && (command == "search" || command == "s" || command == "/")) {
            active_query = prompt("JOURNAL SEARCH> ", ansi_enabled);
            state = {};
            continue;
        }
        if (!archived && (command == "clear" || command == "c")) {
            active_query.clear();
            state = {};
            continue;
        }
        if (const auto choice = vim_menu_choice(command, state, entries.size())) {
            journal_entry_screen(
                store, entries[*choice], archived, profile, ollama_ready, documents, ansi_enabled);
            state = {};
        }
    }
}

void help_screen(
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    shell_header(profile, ollama_ready, documents, ansi_enabled, "COMMAND REFERENCE");
    std::cout << "WORKSTATION CONTROLS\n"
              << "TAB / SHIFT+TAB             navigation / field I/O / find / guide / display\n"
              << "TYPE OR ENTER IN FIELD      run focused local search or Guide query\n"
              << "LAYOUT                      toggle workstation / preserved static model\n\n"
              << "INPUT LOCK CONTROLS\n"
              << "UP/DOWN                     move active menu row\n"
              << "PGDN/PGUP                   move one menu viewport\n"
              << "HOME/END                    first / last item\n"
              << "ENTER                       open active item or text field\n"
              << "1-9 / 0                     direct choices 1-9 / 10\n"
              << "ESC                         return / exit\n\n"
              << "F1 | CARDS                  reviewed emergency cards\n"
              << "F2 | LIBRARY                offline documents + restricted Cookbook shelf\n"
              << "TAB| SCHEMATICS             fixed-cell local text-diagram viewer\n"
              << "F3 | ASK <QUESTION>         retrieve evidence, then optionally use Ollama\n"
              << "F4 | PROFILE                change name, incident, or terrain\n"
              << "F5 | STATUS                 inspect services and terminal resolution\n"
              << "F6 | MAP                    render offline GeoTIFF terrain\n"
              << "F7 | HERBS                  search safety-ranked plants/herbs PDFs\n"
              << "F8 | JOURNAL                Captain's Log and survival help notes\n"
              << "CITIZENS                    resident manifest + private medical-style registry\n"
              << "F9 | SOCIETY                belief, civics, philosophy, economy, rebuilding, archive\n"
              << "F10 | INVENTORY             loadout, health, coordinates, power, and sensors\n"
              << "F11 | OOBE                  guided Civilization Installation\n"
              << "F12 | ABOUT                 WayKeeper mission and credits\n"
              << "SCOUT | UART                master wired Field I/O connection manager\n"
              << "CHAT | MESH                 Nearby BLE mesh build milestone\n"
              << "SEARCH <TERMS>              search every imported source by page\n"
              << "COOKBOOK                   open restricted underground archive shelf\n"
              << "INVENTORY | INV            manage simulated loadout, coordinates, and sensors\n"
              << "LOCK | SENTINEL             enter public privacy screen now\n"
              << "QUIET | BLACKOUT            enter alternate public lock state\n"
              << "SPEED [0.5|1|2]            set Sentinel ANSI-card movie speed\n"
              << "WILL | WILL EDIT            write the local found-device message\n"
              << "WILL ON | WILL OFF          publish/hide it on the Sentinel lockscreen\n"
              << "ESC-W-K-ENTER              hidden wake chord from any Sentinel screen\n"
              << "DEAD-MAN                    five minutes without input locks from any screen\n"
              << "HELP                        show this command reference\n"
              << "CLEAR                       redraw the operations board\n"
              << "QUIT                        exit the local terminal\n";
    pause(ansi_enabled);
}

std::filesystem::path civilization_installer_script(const std::filesystem::path& root) {
    for (const auto& candidate : {
             root / "RES" / "WayKeeper TM" / "waykeeper-civ-install.sh",
             root.parent_path() / "RES" / "WayKeeper TM" / "waykeeper-civ-install.sh"}) {
        if (std::filesystem::is_regular_file(candidate)) return candidate;
    }
    return {};
}

int launch_civilization_installer(
    const std::filesystem::path& script, const std::filesystem::path& target,
    std::string& error) {
    error.clear();
#if defined(__APPLE__) || defined(__unix__)
    std::cout << std::flush;
    const auto process = fork();
    if (process < 0) {
        error = "Could not create installer process.";
        return -1;
    }
    if (process == 0) {
        execl(
            "/bin/bash", "bash", script.c_str(), "--target", target.c_str(),
            static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    while (waitpid(process, &status, 0) < 0) {
        if (errno == EINTR) continue;
        error = "Could not wait for Civilization Installer.";
        return -1;
    }
    if (WIFEXITED(status)) {
        const auto exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            error = "Civilization Installer exited with code " + std::to_string(exit_code) + ".";
        }
        return exit_code;
    }
    error = "Civilization Installer ended unexpectedly.";
    return -1;
#else
    (void)script;
    (void)target;
    error = "The Bash Civilization Installer is not available on this platform.";
    return -1;
#endif
}

std::string read_civilization_record(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return "INSTALLATION RECORD IS NOT AVAILABLE.";
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

void civilization_installation_screen(
    const OperatorProfile& profile, const bool ollama_ready,
    const std::size_t documents, const std::filesystem::path& root,
    const bool ansi_enabled) {
    const auto script = civilization_installer_script(root);
    const auto target = profile_path().parent_path() /
        "civilization" / "waykeeper-encampment";
    for (;;) {
        const bool script_ready = !script.empty();
        const bool installed = std::filesystem::is_directory(target);
        shell_header(
            profile, ollama_ready, documents, ansi_enabled,
            "F11 / OOBE CIVILIZATION INSTALLATION");
        paint(ansi_enabled, ansi::amber_bar);
        std::cout << " WAYKEEPER(TM) CIV-INSTALL // GUIDED MODE ";
        paint(ansi_enabled, ansi::reset);
        std::cout << '\n';
        rule();
        std::cout << "MODULE       " << (script_ready ? "READY" : "MISSING") << '\n'
                  << "INSTALLER    " << (script_ready ? script.string() : "NOT FOUND") << '\n'
                  << "TARGET       " << target.string() << '\n'
                  << "STATE        " << (installed ? "INSTALLED / TARGET LOCKED" : "NOT INSTALLED") << '\n'
                  << "VERSION      0.7.7 / DEPENDENCY-FREE BASH OOBE\n\n"
                  << "The guided installer creates a new encampment charter, people and office\n"
                  << "registers, a field operations list, public ledgers, restricted medical\n"
                  << "records, signatures, and an append-only installation log. It never\n"
                  << "overwrites an existing target.\n\n"
                  << "No network, package manager, sudo, user creation, or system service command\n"
                  << "is executed. Command-like lines inside the installer are thematic display text.\n";
        rule();
        paint(ansi_enabled, ansi::muted);
        if (installed) {
            std::cout << "ACTION  O OPERATIONS LIST  |  V INSTALLED README  |  Q BACK\n";
        } else {
            std::cout << "ACTION  ENTER/L LAUNCH GUIDED INSTALLER  |  Q BACK\n";
        }
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("OOBE> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (installed && (command == "v" || command == "view" || command.empty())) {
            vim_text_screen(
                profile, ollama_ready, documents, ansi_enabled,
                "F11 / INSTALLED CIVILIZATION RECORD",
                "README-FIRST // WAYKEEPER ENCAMPMENT",
                read_civilization_record(target / "README-FIRST.txt"));
            continue;
        }
        if (installed && (command == "o" || command == "operations" || command == "ops")) {
            vim_text_screen(
                profile, ollama_ready, documents, ansi_enabled,
                "F11 / CIVILIZATION OPERATIONS",
                "OPERATIONS LIST // PUBLIC SHIFT BOARD",
                read_civilization_record(target / "OPERATIONS-LIST.txt"));
            continue;
        }
        if (!installed && script_ready &&
            (command.empty() || command == "l" || command == "launch" || command == "install")) {
            std::string launch_error;
            const auto exit_code = launch_civilization_installer(script, target, launch_error);
            clear(ansi_enabled);
            paint(ansi_enabled, exit_code == 0 ? ansi::green : ansi::red);
            std::cout << (exit_code == 0 ? "CIV-INSTALL SESSION COMPLETE" : "CIV-INSTALL FAULT") << '\n';
            if (!launch_error.empty()) std::cout << launch_error << '\n';
            if (std::filesystem::is_directory(target)) {
                std::cout << "ENCAMPMENT RECORD  " << target << '\n';
            } else {
                std::cout << "NO SETTLEMENT FILES DETECTED // INSTALL CANCELLED OR NOT COMMITTED\n";
            }
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        } else if (!script_ready) {
            paint(ansi_enabled, ansi::red);
            std::cout << "INSTALLER NOT FOUND // EXPECTED UNDER RES/WAYKEEPER TM\n";
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        }
    }
}

std::string scout_bytes_for_display(std::string_view bytes, const std::size_t limit = 8192) {
    std::string display;
    display.reserve(std::min(bytes.size(), limit));
    for (const unsigned char value : bytes.substr(0, limit)) {
        if (value == '\n' || value == '\r' || value == '\t' ||
            (std::isprint(value) && value != 0x1b)) {
            display.push_back(static_cast<char>(value));
        } else {
            std::ostringstream escaped;
            escaped << "<" << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << static_cast<int>(value) << ">";
            display += escaped.str();
        }
    }
    if (bytes.size() > limit) display += "\n[OUTPUT TRUNCATED]";
    return display;
}

void scout_configuration(
    ScoutProfile& scout, const OperatorProfile& profile, const bool ansi_enabled) {
    shell_header(
        profile, false, 0, ansi_enabled, "FIELD I/O / MASTER CONNECTION SETUP");
    paint(ansi_enabled, ansi::amber);
    std::cout << "AUTHORIZED EQUIPMENT ONLY // CONFIGURATION DOES NOT TRANSMIT\n";
    paint(ansi_enabled, ansi::reset);
    ScoutProfile candidate = scout;
    const auto name = prompt("PROFILE NAME [" + candidate.name + "]> ", ansi_enabled);
    if (!name.empty()) candidate.name = name;
    const auto transport = lower(prompt(
        "TRANSPORT serial|tcp|telnet|rfc2217|vcom [" +
            scout_transport_name(candidate.transport) + "]> ", ansi_enabled));
    if (!transport.empty() && !parse_scout_transport(transport, candidate.transport)) {
        paint(ansi_enabled, ansi::red);
        std::cout << "UNKNOWN TRANSPORT // CONFIGURATION ABORTED\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    const auto endpoint = prompt(
        (candidate.transport == ScoutTransport::Serial ||
         candidate.transport == ScoutTransport::VirtualCom)
            ? "PORT / DEVICE PATH> " : "HOSTNAME OR IP> ", ansi_enabled);
    if (!endpoint.empty()) candidate.endpoint = endpoint;
    try {
        if (candidate.transport == ScoutTransport::Serial ||
            candidate.transport == ScoutTransport::VirtualCom) {
            const auto baud = prompt("BAUD [" + std::to_string(candidate.baud) + "]> ", ansi_enabled);
            if (!baud.empty()) candidate.baud = std::stoi(baud);
            candidate.port = 0;
        } else {
            const auto port = prompt("TCP PORT [" + std::to_string(candidate.port) + "]> ", ansi_enabled);
            if (!port.empty()) candidate.port = std::stoi(port);
        }
    } catch (...) {
        paint(ansi_enabled, ansi::red);
        std::cout << "INVALID NUMERIC PORT OR BAUD // CONFIGURATION ABORTED\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    const auto adapter = lower(prompt(
        "ADAPTER raw|modem|sensor|ata|ghostline|flipper [" +
            scout_adapter_name(candidate.adapter) + "]> ", ansi_enabled));
    if (!adapter.empty() && !parse_scout_adapter(adapter, candidate.adapter)) {
        paint(ansi_enabled, ansi::red);
        std::cout << "UNKNOWN ADAPTER // CONFIGURATION ABORTED\n";
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    const auto manual = prompt(
        "LOCAL MANUAL SEARCH [" +
            (candidate.manual_query.empty() ? std::string("UNASSIGNED") : candidate.manual_query) +
            "]> ", ansi_enabled);
    if (!manual.empty()) candidate.manual_query = manual;
    candidate.authorized = false;
    candidate.allow_transmit = false;
    std::string error;
    if (!save_scout_profile(candidate, error)) {
        paint(ansi_enabled, ansi::red);
        std::cout << "PROFILE NOT SAVED // " << error << '\n';
    } else {
        scout = std::move(candidate);
        paint(ansi_enabled, ansi::green);
        std::cout << "MASTER CONNECTION SAVED // PASSIVE TEST READY // TX LOCKED\n";
    }
    paint(ansi_enabled, ansi::reset);
    pause(ansi_enabled);
}

void ghostline_control_screen(
    ScoutProfile& scout, const OperatorProfile& profile,
    const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    const std::vector<std::string> actions{
        "START OBSERVE-ONLY RELAY + MICRO CAPTURE",
        "STOP RELAY // PRESERVE CAPTURE ARTIFACTS",
        "VIEW TERMINAL HEX / ASCII CAPTURE",
        "VIEW GHOSTLINE PROCESS LOG",
        "FIELD PROFILE + CONNACK + COM0COM NOTES",
        "RETURN TO UART SCOUT",
    };
    VimMenuState state;
    for (;;) {
        const auto runtime = ghostline_status(scout);
        shell_header(profile, ollama_ready, documents, ansi_enabled,
                     "FIELD I/O / GHOSTLINE MICRO CAPTURE");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "GHOSTLINE OBSERVER\n";
        paint(ansi_enabled, ansi::reset);
        std::cout << "  INSTALL     " << (runtime.installed ? "READY" : "NOT FOUND") << '\n'
                  << "  PROCESS     " << (runtime.running ? "RUNNING" : "STOPPED")
                  << "  | PID " << runtime.process_id << '\n'
                  << "  LOCAL I/O   " << runtime.local_endpoint << '\n'
                  << "  MASTER      " << scout_transport_name(scout.transport) << " // "
                  << scout.endpoint;
        if (scout.port > 0) std::cout << ':' << scout.port;
        std::cout << " // " << scout.baud << " BAUD\n"
                  << "  ACCESS      " << (scout.authorized ? "AUTHORIZED" : "LOCKED")
                  << "  | MODE OBSERVE-ONLY\n"
                  << "  GLCAP       " << runtime.capture_path << '\n'
                  << "  PCAP        " << runtime.pcap_path << " // DLT_USER0\n"
                  << "  STATUS      " << runtime.detail << '\n';
        rule();
        const auto rows = vim_menu_rows(24);
        render_vim_menu(actions, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "CAPTURE CEILING 4 MIB / RUN | SNAPLEN 1024 | NO PROMISCUOUS MODE | NO ROOT\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("GHOSTLINE> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, actions.size(), rows)) continue;
        const auto choice = vim_menu_choice(command, state, actions.size());
        if (!choice) continue;
        if (*choice == 5) return;
        if (*choice == 0) {
            GhostlineRuntime started;
            std::string error;
            const bool success = start_ghostline_observer(scout, started, error);
            paint(ansi_enabled, success ? ansi::green : ansi::red);
            std::cout << (success
                ? "GHOSTLINE STARTED // CONNECT CLIENT TO " + started.local_endpoint
                : "START FAULT // " + error) << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        } else if (*choice == 1) {
            auto stopped = runtime;
            std::string error;
            const bool success = stop_ghostline_observer(stopped, error);
            paint(ansi_enabled, success ? ansi::green : ansi::red);
            std::cout << (success ? stopped.detail : "STOP FAULT // " + error) << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        } else if (*choice == 2) {
            std::string rendered;
            std::string error;
            if (!render_ghostline_capture(runtime.capture_path, 48, rendered, error)) {
                rendered = "CAPTURE UNAVAILABLE // " + error;
            }
            vim_text_screen(profile, ollama_ready, documents, ansi_enabled,
                            "FIELD I/O / MICRO WIRESHARK", "LAST 48 STREAM RECORDS", rendered);
        } else if (*choice == 3) {
            std::ifstream input(runtime.log_path, std::ios::binary);
            std::string log;
            if (input) {
                log.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
                if (log.size() > 65536) {
                    log = "[LOG TAIL TRUNCATED TO 64 KIB]\n" + log.substr(log.size() - 65536);
                }
            } else log = "PROCESS LOG UNAVAILABLE // START GHOSTLINE FIRST.";
            vim_text_screen(profile, ollama_ready, documents, ansi_enabled,
                            "FIELD I/O / GHOSTLINE LOG", "LOCAL PROCESS OUTPUT", log);
        } else if (*choice == 4) {
            constexpr std::string_view notes = R"GHOST(GHOSTLINE FIELD PROFILE

TCP
  Ghostline listens only on 127.0.0.1:17777 and relays to the one approved
  master target. Capture occurs before plugin decisions. WayKeeper always starts
  this adapter with --observe-only, so original bytes remain on the delivery path.

MQTT CONNACK
  Port 1883 arms MQTT framing and CONNACK summaries. The terminal shows Session
  Present plus the reason code. TLS MQTT on 8883 remains encrypted and is not
  decoded unless termination occurs in an explicitly configured application.

SERIAL / BAUD
  A local serial profile becomes TCP 127.0.0.1:17777 to the configured UART/vCOM
  endpoint at its selected baud. Ghostline standalone profiles also support a
  serial-to-serial pair with different ingress and upstream baud rates.

COM0COM
  Seeded Ghostline profiles contain COM5 <-> COM6 placeholders. com0com creates
  the Windows virtual pair; Ghostline treats each endpoint as a serial stream.
  The present serial runtime is macOS/Linux. Win32 serial execution remains
  pending, so the Windows profile is a prepared contract, not a false READY.

CAPTURE
  GLCAP1 is recovery-friendly text with timestamp, flow, direction, summary,
  payload hex, and ASCII. PCAP uses DLT_USER0 and opens in Wireshark as user data.
  It is application-stream capture, not promiscuous NIC capture or full IP headers.
)GHOST";
            vim_text_screen(profile, ollama_ready, documents, ansi_enabled,
                            "FIELD I/O / GHOSTLINE NOTES", "OFFLINE OPERATIONS", notes);
        }
    }
}

void mesh_chat_console_screen(
    const MeshProfile& mesh, const OperatorProfile& operator_profile,
    const bool ollama_ready, const std::size_t documents, const bool ansi_enabled) {
    std::vector<std::string> session_lines;
    bool entering_console = true;
    for (;;) {
        const auto readiness = inspect_mesh_readiness();
        shell_header(
            operator_profile, ollama_ready, documents, ansi_enabled,
            "NEARBY BLE CHAT / CONSOLE");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "PUBLIC NEARBY ROOM // " << upper(mesh.nickname)
                  << " // " << mesh.channel << '\n';
        paint(ansi_enabled, readiness.protocol_backend_ready ? ansi::green : ansi::red);
        std::cout << "MODE " << mesh_power_mode_name(mesh.requested_mode)
                  << " REQUESTED // RADIO "
                  << (readiness.protocol_backend_ready ? "READY" : "TX LOCKED") << '\n';
        paint(ansi_enabled, ansi::muted);
        std::cout << "SESSION VIEW IS RAM-ONLY // NO MESSAGE IS QUEUED WHILE TX IS LOCKED\n";
        paint(ansi_enabled, ansi::reset);
        rule();

        const auto terminal = terminal_size();
        const auto columns = std::clamp<std::size_t>(
            terminal.columns > 2 ? terminal.columns - 2 : terminal.columns, 30, width);
        const auto visible_rows = std::clamp<std::size_t>(
            terminal.rows > 14 ? terminal.rows - 14 : 6, 6, 18);
        std::ostringstream transcript;
        if (session_lines.empty()) {
            transcript << "NO RECEIVED MESSAGES // TYPE BELOW TO TEST THE CHAT SURFACE";
        } else {
            for (const auto& line : session_lines) transcript << line << '\n';
        }
        const auto rendered_lines = pager_lines(transcript.str(), columns);
        const auto start = rendered_lines.size() > visible_rows
            ? rendered_lines.size() - visible_rows : 0;
        for (auto index = start; index < rendered_lines.size(); ++index) {
            std::cout << rendered_lines[index] << '\n';
        }
        for (auto row = rendered_lines.size() - start; row < visible_rows; ++row) {
            std::cout << '\n';
        }
        rule();
        paint(ansi_enabled, ansi::cyan);
        std::cout << "CHAT CLI  TYPE MESSAGE  |  /BACK RETURN  |  /CLEAR LOCAL VIEW\n";
        paint(ansi_enabled, ansi::muted);
        std::cout << "EMPTY ENTER RETURNS AFTER THE ENTRY HANDOFF // 512-BYTE MAXIMUM\n";
        paint(ansi_enabled, ansi::reset);

        auto message = prompt("BLE CHAT> ", ansi_enabled);
        if (message.empty()) {
            if (entering_console) {
                entering_console = false;
                continue;
            }
            return;
        }
        entering_console = false;
        const auto command = lower(message);
        if (command == "/back" || command == "/exit" || command == "/quit") return;
        if (command == "/clear") {
            session_lines.clear();
            continue;
        }
        if (message.size() > mesh_message_max_bytes) {
            session_lines.push_back("[ERROR] MESSAGE EXCEEDS 512 BYTES // NOT SENT");
            continue;
        }
        session_lines.push_back(
            std::string(readiness.protocol_backend_ready
                ? "[LOCAL TEST / TX ADAPTER MISSING] "
                : "[LOCAL TEST / TX LOCKED / NOT SENT] ") +
            upper(mesh.nickname) + "> " + message);
    }
}

void mesh_chat_screen(
    const OperatorProfile& operator_profile, const bool ollama_ready,
    const std::size_t documents, const bool ansi_enabled) {
    VimMenuState state;
    MeshProfile mesh;
    std::string error;
    if (!load_mesh_profile(mesh, error)) {
        paint(ansi_enabled, ansi::red);
        std::cout << error << '\n';
        paint(ansi_enabled, ansi::reset);
        pause(ansi_enabled);
        return;
    }
    std::string chat_notice;
    bool entering_screen = true;
    for (;;) {
        const auto readiness = inspect_mesh_readiness();
        auto actions = mesh_sidebar_actions(mesh);
        actions[1] = "NICKNAME // " + upper(mesh.nickname);
        shell_header(
            operator_profile, ollama_ready, documents, ansi_enabled,
            "NEARBY CHAT");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "WAYKEEPER NEARBY CHAT\n";
        paint(ansi_enabled, ansi::reset);
        std::cout << "  " << upper(mesh.nickname) << "  |  " << mesh.channel << "  |  "
                  << mesh_power_mode_name(mesh.requested_mode) << " REQUESTED\n"
                  << "  RADIO "
                  << (readiness.protocol_backend_ready ? "READY" : "LOCKED UNTIL ORANGE PI TEST")
                  << "  |  PUBLIC NEARBY ROOM\n";
        if (!chat_notice.empty()) {
            paint(ansi_enabled, ansi::amber);
            std::cout << "  " << chat_notice << '\n';
            paint(ansi_enabled, ansi::reset);
        }
        rule();
        const auto rows = std::size_t{6};
        render_vim_menu(actions, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "ITEM 4 OPENS BOTTOM CHAT INPUT  |  ITEM 5 BACK  |  INPUT LOCK ACTIVE\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("RADIO> ", ansi_enabled, true);
        if (entering_screen && command.empty()) {
            entering_screen = false;
            continue;
        }
        entering_screen = false;
        if (command == "q" || command == "back" || command == "escape" || command == "0") return;
        if (navigate_vim_menu(command, state, actions.size(), rows)) continue;
        const auto choice = vim_menu_choice(command, state, actions.size());
        if (!choice) continue;
        if (*choice == 4) return;
        if (*choice == 0) {
            mesh.requested_mode = mesh.requested_mode == MeshPowerMode::Off
                ? MeshPowerMode::Eco
                : mesh.requested_mode == MeshPowerMode::Eco
                    ? MeshPowerMode::Active : MeshPowerMode::Off;
            if (!save_mesh_profile(mesh, error)) {
                paint(ansi_enabled, ansi::red);
                std::cout << error << '\n';
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            }
        } else if (*choice == 1) {
            const auto nickname = prompt("MESH NICKNAME [A-Z 0-9 - _]> ", ansi_enabled);
            if (!valid_mesh_nickname(nickname)) {
                paint(ansi_enabled, ansi::red);
                std::cout << "NICKNAME MUST BE 1-24 LETTERS, DIGITS, '-' OR '_'\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            } else {
                mesh.nickname = nickname;
                if (!save_mesh_profile(mesh, error)) {
                    paint(ansi_enabled, ansi::red);
                    std::cout << error << '\n';
                    paint(ansi_enabled, ansi::reset);
                    pause(ansi_enabled);
                }
            }
        } else if (*choice == 2) {
            std::ostringstream report;
            report << "RADIO       "
                   << (readiness.protocol_backend_ready ? "READY" : "LOCKED") << '\n'
                   << "BLUEZ       " << (readiness.bluez_runtime ? "DETECTED" : "NOT DETECTED") << '\n'
                   << "CONTROLLER  " << readiness.controller << "\n\n"
                   << readiness.detail << "\n\n"
                      "No message is sent until the Orange Pi interoperability test passes.\n"
                      "Public nearby messages and radio presence are observable.\n";
            vim_text_screen(
                operator_profile, ollama_ready, documents, ansi_enabled,
                "NEARBY CHAT / STATUS", "RADIO STATUS", report.str());
        } else if (*choice == 3) {
            mesh_chat_console_screen(
                mesh, operator_profile, ollama_ready, documents, ansi_enabled);
            chat_notice = "CHAT CONSOLE CLOSED // LOCAL SESSION VIEW DISCARDED";
        }
    }
}

void io_scout_screen(
    SurvivalLibrary& library, const OperatorProfile& profile,
    const bool ollama_ready, const bool ansi_enabled) {
    const std::vector<std::string> actions{
        "CONFIGURE MASTER CONNECTION // TARGET + TRANSPORT + ADAPTER",
        "DISCOVER LOCAL COM / USB-UART PORTS",
        "FIND TARGET MANUAL IN OFFLINE ARCHIVE",
        "PASSIVE CARRIER TEST // SERIAL OPEN OR TCP CONNECT",
        "PASSIVE SERIAL READOUT // NO PAYLOAD",
        "PASSIVE BAUD SCAN // PRINTABLE + FRAME SCORE",
        "RECORD AUTHORIZATION // TARGET OWNED OR APPROVED",
        "TOGGLE TRANSMIT GATE // SEPARATE FROM CONNECT",
        "SEND EXACT COMMAND / PAYLOAD // RESPONSE READOUT",
        "GRID RESTORATION WATCH // LOCAL LINK + APPROVED TARGET",
        "PROTOCOL / BUS / BUSYBOX OPERATIONS LIST",
        "GHOSTLINE MICRO CAPTURE // START / STOP / HEX / PCAP",
        "RETURN TO NAVIGATION",
    };
    VimMenuState state;
    ScoutProfile scout;
    std::string load_error;
    bool configured = load_scout_profile(scout, load_error);
    for (;;) {
        shell_header(profile, ollama_ready, library.documents().size(), ansi_enabled,
                     "FIELD I/O / UART SCOUT");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "MASTER CONNECTION\n";
        paint(ansi_enabled, ansi::reset);
        if (configured) {
            std::cout << "  PROFILE     " << upper(scout.name) << '\n'
                      << "  TRANSPORT   " << scout_transport_name(scout.transport)
                      << "  | ADAPTER " << scout_adapter_name(scout.adapter) << '\n'
                      << "  ENDPOINT    " << scout.endpoint;
            if (scout.port > 0) std::cout << ':' << scout.port;
            std::cout << "  | " << scout.baud << " BAUD / " << scout.data_bits
                      << upper(scout.parity.substr(0, 1)) << scout.stop_bits << '\n'
                      << "  MANUAL      " << (scout.manual_query.empty() ? "UNASSIGNED" : scout.manual_query) << '\n'
                      << "  ACCESS      " << (scout.authorized ? "AUTHORIZED" : "UNRECORDED")
                      << "  | TX " << (scout.allow_transmit ? "ENABLED" : "LOCKED") << '\n';
        } else {
            paint(ansi_enabled, ansi::amber);
            std::cout << "  NOT CONFIGURED // CREATE ONE PROFILE; ALL FEATURE ADAPTERS REUSE IT\n";
            paint(ansi_enabled, ansi::reset);
        }
        rule();
        const auto rows = vim_menu_rows(23);
        render_vim_menu(actions, state, rows, ansi_enabled);
        paint(ansi_enabled, ansi::muted);
        std::cout << "POLICY  AUTHORIZED DEVICES ONLY | DISCOVERY IS LOCAL | NO NETWORK RANGE SCANNING\n";
        paint(ansi_enabled, ansi::reset);
        const auto command = pager_prompt("UART-SCOUT> ", ansi_enabled);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, actions.size(), rows)) continue;
        const auto choice = vim_menu_choice(command, state, actions.size());
        if (!choice) continue;
        if (*choice == 12) return;
        if (*choice == 0) {
            scout_configuration(scout, profile, ansi_enabled);
            configured = load_scout_profile(scout, load_error);
            continue;
        }
        if (*choice == 1) {
            const auto endpoints = discover_scout_endpoints();
            std::ostringstream report;
            report << "LOCAL SERIAL ENDPOINTS: " << endpoints.size() << "\n\n";
            for (const auto& endpoint : endpoints) {
                report << endpoint.path << "\n  " << endpoint.kind << " // "
                       << endpoint.detail << "\n";
            }
            if (endpoints.empty()) report << "NO LOCAL COM OR USB-UART ENDPOINTS DETECTED.\n";
            vim_text_screen(profile, ollama_ready, library.documents().size(), ansi_enabled,
                            "FIELD I/O / PORT DISCOVERY", "LOCAL ENUMERATION ONLY",
                            report.str());
            continue;
        }
        if (!configured) {
            paint(ansi_enabled, ansi::red);
            std::cout << "CONFIGURE A MASTER CONNECTION FIRST\n";
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
            continue;
        }
        if (*choice == 2) {
            auto query = scout.manual_query;
            if (query.empty()) query = prompt("MANUAL SEARCH> ", ansi_enabled);
            if (!query.empty()) search_screen(
                library, profile, ollama_ready, ansi_enabled, query);
        } else if (*choice == 3) {
            const auto result = probe_scout_profile(scout);
            paint(ansi_enabled, result.reachable ? ansi::green : ansi::red);
            std::cout << result.status << " // " << result.detail << '\n';
            paint(ansi_enabled, ansi::reset);
            pause(ansi_enabled);
        } else if (*choice == 4) {
            std::string bytes;
            std::string error;
            if (!scout_read(scout, 2500, bytes, error)) bytes = "READ FAULT // " + error;
            else if (bytes.empty()) bytes = "NO BYTES OBSERVED IN 2500 MS.";
            else bytes = scout_bytes_for_display(bytes);
            vim_text_screen(profile, ollama_ready, library.documents().size(), ansi_enabled,
                            "FIELD I/O / PASSIVE READOUT", "NO PAYLOAD TRANSMITTED", bytes);
        } else if (*choice == 5) {
            std::string error;
            const auto results = passive_baud_scan(scout, 300, error);
            std::ostringstream report;
            report << "PASSIVE BAUD SCAN // 300 MS PER RATE\n"
                      "CAUTION: OPENING SOME USB-UART ADAPTERS MAY TOGGLE MODEM CONTROL LINES.\n\n";
            for (const auto& result : results) {
                report << std::setw(6) << result.baud << "  SCORE " << std::setw(3)
                       << result.score << "  BYTES " << std::setw(5) << result.bytes_observed
                       << "  PRINTABLE " << std::fixed << std::setprecision(0)
                       << result.printable_ratio * 100.0 << "%\n";
            }
            if (results.empty()) report << "SCAN FAULT // " << error << '\n';
            vim_text_screen(profile, ollama_ready, library.documents().size(), ansi_enabled,
                            "FIELD I/O / BAUD SCAN", "LISTEN-ONLY HEURISTIC", report.str());
        } else if (*choice == 6) {
            const auto confirmation = upper(prompt(
                "TYPE I OWN OR AM AUTHORIZED> ", ansi_enabled));
            if (confirmation == "I OWN OR AM AUTHORIZED") {
                scout.authorized = true;
                scout.allow_transmit = false;
                std::string error;
                configured = save_scout_profile(scout, error);
            }
        } else if (*choice == 7) {
            if (!scout.authorized) {
                paint(ansi_enabled, ansi::red);
                std::cout << "TX LOCKED // RECORD TARGET AUTHORIZATION FIRST\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            } else {
                const auto confirmation = upper(prompt(
                    scout.allow_transmit ? "TYPE LOCK TX> " : "TYPE ENABLE TX> ", ansi_enabled));
                if (confirmation == (scout.allow_transmit ? "LOCK TX" : "ENABLE TX")) {
                    scout.allow_transmit = !scout.allow_transmit;
                    std::string error;
                    configured = save_scout_profile(scout, error);
                }
            }
        } else if (*choice == 8) {
            const auto payload = prompt("EXACT PAYLOAD (NO NEWLINE ADDED)> ", ansi_enabled);
            std::string response;
            std::string error;
            if (!scout_send(scout, payload, 1500, response, error)) {
                response = "TX FAULT // " + error;
            } else {
                response = "TX COMPLETE // RX " + std::to_string(response.size()) +
                    " BYTES\n\n" + scout_bytes_for_display(response);
            }
            vim_text_screen(profile, ollama_ready, library.documents().size(), ansi_enabled,
                            "FIELD I/O / TERMINAL", "EXPLICIT OPERATOR TRANSMIT", response);
        } else if (*choice == 9) {
            const auto network = inspect_network();
            const auto probe = probe_scout_profile(scout);
            std::ostringstream report;
            report << "GRID RESTORATION WATCH // APPROVED TARGETS ONLY\n\n"
                   << "LOCAL LINK     " << (network.online ? "ONLINE" : "OFFLINE") << '\n'
                   << "INTERFACE      " << (network.interface_name.empty() ? "NONE" : network.interface_name) << '\n'
                   << "ADDRESS        " << (network.address.empty() ? "NONE" : network.address) << '\n'
                   << "MASTER TARGET  " << probe.status << '\n'
                   << "DETAIL         " << probe.detail << "\n\n"
                      "THIS WATCH DOES NOT SWEEP SUBNETS, ENUMERATE UTILITY EQUIPMENT, OR ATTEMPT LOGIN.\n"
                      "ADD INDIVIDUAL INFRASTRUCTURE ENDPOINTS ONLY WHEN YOU OWN THEM OR HAVE AUTHORIZATION.\n";
            vim_text_screen(profile, ollama_ready, library.documents().size(), ansi_enabled,
                            "FIELD I/O / GRID WATCH", "PASSIVE / EXPLICIT TARGET", report.str());
        } else if (*choice == 10) {
            constexpr std::string_view operations = R"OPS(OPERATIONS TAXONOMY

TRANSPORTS
  SERIAL / USB-UART     Local asynchronous byte stream
  TCP / ETHERNET        Network carrier to one explicit host and port
  TELNET                Character session over TCP; negotiation required
  RFC2217               Telnet COM-port control; baud/framing can be negotiated
  VIRTUAL COM           OS port provider such as Windows com0com

ADAPTERS / SERVICES
  RAW CONSOLE           Exact byte terminal
  MODEM / AT            Hayes-style AT command adapter
  SENSOR / OGP1         WayKeeper telemetry line parser
  ATA / SIP / VOIP      Telephony service adapter; not a serial transport
  GHOSTLINE TCP         External TCP observation/mutation workbench
  FLIPPER ZERO          Optional USB CLI or USB-UART bridge

HARDWARE BUSES
  UART / RS-232 / RS-485 / CAN / I2C / SPI need the correct voltage-level,
  isolation, and bus adapter. Never connect raw UART directly to Ethernet or USB.

BUSYBOX
  BusyBox is useful as an optional field toolbox (ping, nc, telnet, ip). WayKeeper
  does not silently invoke it and does not grant shell access to a target device.
)OPS";
            vim_text_screen(profile, ollama_ready, library.documents().size(), ansi_enabled,
                            "FIELD I/O / OPERATIONS", "TRANSPORTS ARE NOT SERVICES", operations);
        } else if (*choice == 11) {
            ghostline_control_screen(
                scout, profile, ollama_ready, library.documents().size(), ansi_enabled);
        }
    }
}

void field_io_hub_screen(
    SurvivalLibrary& library, const OperatorProfile& profile,
    const bool ollama_ready, const bool ansi_enabled) {
    const std::vector<std::string> actions{
        "UART SCOUT // AUTHORIZED WIRED FIELD I/O",
        "NEARBY BLE MESH // ANSI BUILD MILESTONE",
        "RETURN TO COMMAND CENTER",
    };
    VimMenuState state;
    for (;;) {
        shell_header(
            profile, ollama_ready, library.documents().size(), ansi_enabled,
            "FIELD I/O / UART + BLE");
        paint(ansi_enabled, ansi::cyan);
        std::cout << "SELECT A DELIBERATE TRANSPORT WORKSPACE\n";
        paint(ansi_enabled, ansi::reset);
        rule();
        render_vim_menu(actions, state, vim_menu_rows(10), ansi_enabled);
        const auto command = pager_prompt("FIELD-I/O> ", ansi_enabled, true);
        if (command == "q" || command == "back" || command == "escape") return;
        if (navigate_vim_menu(command, state, actions.size(), 10)) continue;
        const auto choice = vim_menu_choice(command, state, actions.size());
        if (!choice || *choice == 2) {
            if (choice) return;
            continue;
        }
        if (*choice == 0) io_scout_screen(library, profile, ollama_ready, ansi_enabled);
        else mesh_chat_screen(
            profile, ollama_ready, library.documents().size(), ansi_enabled);
    }
}

void about_screen(
    const OperatorProfile& profile, const bool ollama_ready, const std::size_t documents,
    const bool ansi_enabled) {
    constexpr std::string_view about = R"ABOUT(ABOUT WAYKEEPER
===============================================================================

SCENARIO           END OF HUMANITY
CONDITION          NUCLEAR FALLOUT
PRIMARY DIRECTIVE  SURVIVE. PRESERVE. REBUILD.

CREDITS
-------------------------------------------------------------------------------

A Michael Cohee Production
WayKeeper Labs
A Division of Premise-i3 LLC

It is with great pleasure - and even greater care - that I have enclosed herein a compendium, an atlas, and a living series of tomes, instruments, and survival features dedicated to the preservation of humankind and, more immediately, to the preservation of you.

The knowledge archived within WayKeeper descends from field manuals, survival doctrine, medical references, maps, recovered techniques, and the hard-won testimony of those who endured war, captivity, disaster, isolation, and terror. Such knowledge has guided the lost, sustained prisoners of war, and helped bring people home.

WayKeeper carries that same principle forward:

WHEN THE WORLD CAN NO LONGER HELP YOU, KNOWLEDGE MUST.

This is more than a library. It is a survival instrument for the world that remains—a keeper of routes, remedies, signals, shelters, histories, skills, and human memory.

No archive can promise survival. No machine can replace judgment. But preparation can extend the light, preserve the hearth, and give humanity another day in which to endure.

Study these records before they are needed.
Preserve what must not be forgotten.
Protect those who remain.
Return home whenever you can.

And if home no longer stands -

BUILD IT AGAIN.

"Make it a great day."

Sir Frater and Magus Michael Anthony
Anointed Michael, Archangel Cohee
Founder, WayKeeper Labs
Premise-i3 LLC
)ABOUT";
    const auto root = resource_root();
    CreditsMusicLoop music(root);
    const std::string heading = "ABOUT WAYKEEPER // WASTELAND RADIO " +
        std::string(music.active() ? "LOOP ON / " : "UNAVAILABLE / ") +
        std::to_string(music.track_count()) + " TRACKS";
    vim_text_screen(
        profile, ollama_ready, documents, ansi_enabled,
        "F12 / ABOUT + CREDITS", heading, about);
}

std::string shell_prompt(const OperatorProfile& profile) {
    auto compact = [](std::string value) {
        const auto slash = value.find(" /");
        if (slash != std::string::npos) value.resize(slash);
        value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char c) {
            return std::isspace(c);
        }), value.end());
        return lower(value);
    };
    return "OG/" + compact(profile.name) + "@" + compact(profile.terrain) + ":" +
        compact(profile.incident) + "> ";
}

}  // namespace

int run_survival_ui() {
    const bool ansi_enabled = enable_ansi_terminal();
    FixedTerminalScreen fixed_screen(ansi_enabled);
    auto& settings = ui_settings();
    std::string settings_error;
    const bool settings_exist = std::filesystem::exists(settings_path());
    const bool settings_loaded = load_terminal_settings(settings, settings_error);
    if (!settings_loaded) settings = {};
    if (!settings_exist || !settings_loaded) save_terminal_settings(settings, settings_error);
    apply_terminal_theme(settings.theme);
    if (settings.resize_on_launch) {
        request_terminal_pixel_size(settings.width_pixels, settings.height_pixels, ansi_enabled);
    }
    clear_launch_window(ansi_enabled);
    set_title(ansi_enabled);
    const auto root = resource_root();
    splash_screen(ansi_enabled);
    std::string boot_unlock_error;
    const auto pending_boot_unlock = consume_boot_unlock(boot_unlock_path(), boot_unlock_error);
    if (pending_boot_unlock != BootUnlock::None) {
        boot_unlock_splash(
            pending_boot_unlock, root, ansi_enabled, boot_unlock_error);
    }
    SurvivalLibrary library;
    std::string library_error;
    const bool library_ready = library.load(root / "library" / "catalog.tsv", library_error);
    OllamaClient ollama;

    OperatorProfile profile;
    std::string profile_error;
    if (!load_profile(profile, profile_error)) profile = onboarding(ansi_enabled);

    InventoryStore inventory_store(inventory_path());
    InventoryState inventory;
    std::string inventory_error;
    if (!inventory_store.load(inventory, inventory_error)) {
        inventory = default_inventory_state();
        inventory_store.save(inventory, inventory_error);
    }
    InventorySummary inventory_summary = summarize_inventory(inventory);
    if (!std::filesystem::exists(inventory_store.summary_path())) {
        inventory_store.write_summary(inventory, inventory_summary, inventory_error);
    }
    deadman_controller().arm();

    const std::vector<std::string> command_center_items{
        "F1  CARDS    // REVIEWED ZERO-INFERENCE ANSWERS",
        "F2  DOCS     // LIBRARY + SCHEMATICS + ARCHIVES",
        "F3  GUIDE    // EVIDENCE-FIRST LOCAL ASSISTANT",
        "F4  PROFILE  // OPERATOR / INCIDENT / TERRAIN",
        "F5  SYSTEM   // LOCAL SERVICES AND READINESS",
        "F6  MAP      // OFFLINE USGS TERRAIN + TRAILS",
        "F7  HERBS    // PLANTS / HERBS EVIDENCE DATABASE",
        "F8  JOURNAL  // CAPTAIN'S LOG + CITIZENS + HELP NOTES",
        "F9  SOCIETY  // BELIEF / CIVICS / PHILOSOPHY / FOIA",
        "F10 INVENTORY // HEALTH / LOADOUT / POWER / SENSORS",
        "F11 OOBE     // GUIDED CIVILIZATION INSTALLATION",
        "F12 ABOUT    // WAYKEEPER MISSION + CREDITS",
    };
    VimMenuState command_center_state;
    WorkstationFocus workstation_focus = WorkstationFocus::Navigation;
    std::string archive_query;
    std::string guide_query;
    const auto switch_display_mode = [&](const bool minimal) {
        settings.layout_mode = minimal ? "minimal" : "workstation";
        settings.width_pixels = 640;
        settings.height_pixels = 480;
        std::string display_error;
        (void)save_terminal_settings(settings, display_error);
        request_terminal_pixel_size(
            settings.width_pixels, settings.height_pixels, ansi_enabled);
        clear_launch_window(ansi_enabled);
        workstation_focus = WorkstationFocus::Navigation;
    };

    for (;;) {
        try {
        const bool ollama_ready = ollama.server_ready();
        const std::size_t document_count = library_ready ? library.documents().size() : 0;
        shell_header(profile, ollama_ready, document_count, ansi_enabled, "COMMAND CENTER");
        if (!library_ready) {
            paint(ansi_enabled, ansi::red);
            std::cout << "\nLIBRARY FAULT: " << library_error << '\n';
            paint(ansi_enabled, ansi::reset);
        }
        const bool minimal_mode = settings.layout_mode == "minimal";
        const bool workstation = settings.layout_mode == "workstation";
        const bool lcd_compact = compact_display();
        const bool tabbed_layout = workstation || minimal_mode;
        if (tabbed_layout) {
            render_workstation_fields(
                workstation_focus, archive_query, guide_query,
                minimal_mode || lcd_compact, ansi_enabled);
        }
        const auto rows = lcd_compact ? command_center_items.size()
                                      : minimal_mode ? std::size_t{8}
                                                     : command_center_items.size();
        if (lcd_compact) {
            render_compact_command_center(
                command_center_items, command_center_state, ansi_enabled);
            render_minimal_status(inventory, inventory_summary, ansi_enabled);
        } else if (minimal_mode) {
            render_minimal_command_center(
                command_center_items, command_center_state, ansi_enabled);
            render_minimal_status(inventory, inventory_summary, ansi_enabled);
        } else {
            render_command_center_grid(
                command_center_items, command_center_state, profile, ansi_enabled);
            if (workstation) {
                render_workstation_graphics(inventory, inventory_summary, ansi_enabled);
            }
            render_inventory_dashboard(inventory, inventory_summary, ansi_enabled);
        }
        paint(ansi_enabled, ansi::muted);
        const std::string escape_action = appliance_mode() ? "ESC LOCK" : "ESC EXIT";
        expanded_line(
            lcd_compact
                ? "640X480 LCD | INPUT LOCK | ENTER | TAB | 0-9 | F1-F12 | " + escape_action
                : minimal_mode
                ? "INPUT LOCK | ENTER OPEN | TAB FOCUS | F1-F12 DIRECT | " + escape_action
                : "INVENTORY :INVENTORY | SIMULATION DATA | LAST " + inventory.updated_at +
                    " | INPUT LOCK | ENTER | TAB | 0-9 | F1-F12 | " + escape_action,
            interface_width());
        paint(ansi_enabled, ansi::reset);

        auto normalized = pager_prompt(
            tabbed_layout ? (workstation_focus == WorkstationFocus::Navigation ? "WK/NAV> " :
                workstation_focus == WorkstationFocus::FieldIo ? "WK/I/O> " :
                workstation_focus == WorkstationFocus::ArchiveSearch ? "WK/FIND> " :
                workstation_focus == WorkstationFocus::GuideQuery ? "WK/ASK> " :
                workstation_focus == WorkstationFocus::Schematics ? "WK/SCHEM> " : "WK/MODE> ") :
                shell_prompt(profile), ansi_enabled, true);
        if (tabbed_layout && (normalized == "tab" || normalized == "backtab")) {
            workstation_focus = next_workstation_focus(
                workstation_focus, normalized == "backtab");
            continue;
        }
        if (tabbed_layout && workstation_focus == WorkstationFocus::DisplayMode &&
            normalized.empty()) {
            switch_display_mode(!minimal_mode);
            continue;
        }
        if (tabbed_layout && workstation_focus == WorkstationFocus::FieldIo &&
            normalized.empty()) {
            if (library_ready) {
                field_io_hub_screen(library, profile, ollama_ready, ansi_enabled);
            }
            continue;
        }
        if (tabbed_layout && workstation_focus == WorkstationFocus::Schematics &&
            normalized.empty()) {
            schematics_screen(
                root, profile, ollama_ready, document_count, ansi_enabled);
            continue;
        }
        if (normalized == "minimal" || normalized == "min") {
            switch_display_mode(true);
            continue;
        }
        if (normalized == "full" || normalized == "workstation") {
            switch_display_mode(false);
            continue;
        }
        if (tabbed_layout &&
            (workstation_focus == WorkstationFocus::ArchiveSearch ||
             workstation_focus == WorkstationFocus::GuideQuery) &&
            normalized.empty()) {
            const std::string seed;
            if (workstation_focus == WorkstationFocus::ArchiveSearch) {
                archive_query = prompt_seeded("ARCHIVE FIND> ", seed, ansi_enabled);
                if (!archive_query.empty()) normalized = "search " + archive_query;
                else continue;
            } else {
                guide_query = prompt_seeded("GUIDE QUERY> ", seed, ansi_enabled);
                if (!guide_query.empty()) normalized = "ask " + guide_query;
                else continue;
            }
        }
        if ((!tabbed_layout || workstation_focus == WorkstationFocus::Navigation) && navigate_vim_menu(
                normalized, command_center_state, command_center_items.size(), rows)) {
            continue;
        }
        if ((!tabbed_layout || workstation_focus == WorkstationFocus::Navigation)) {
            if (const auto choice = vim_menu_choice(
                normalized, command_center_state, command_center_items.size())) {
                normalized = "f" + std::to_string(*choice + 1);
            }
        }
        if (normalized == "will" || normalized == "will edit") {
            (void)edit_last_will_message(
                profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "will on" || normalized == "will off") {
            const bool enable = normalized == "will on";
            std::string will_error;
            if (enable && !load_sentinel_last_will(
                    sentinel_last_will_path(), will_error)) {
                paint(ansi_enabled, ansi::red);
                std::cout << "LAST WILL NOT ENABLED // WRITE AND SAVE A MESSAGE FIRST\n";
                if (!will_error.empty()) std::cout << will_error << '\n';
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            } else {
                settings.sentinel_last_will_enabled = enable;
                if (!save_terminal_settings(settings, will_error)) {
                    paint(ansi_enabled, ansi::red);
                    std::cout << will_error << '\n';
                    paint(ansi_enabled, ansi::reset);
                    pause(ansi_enabled);
                }
            }
        } else if (normalized == "lock" || normalized == "sentinel") {
            sentinel_idle_screen(root, SentinelMode::Sentinel, ansi_enabled);
        } else if (normalized == "quiet") {
            sentinel_idle_screen(root, SentinelMode::Quiet, ansi_enabled);
        } else if (normalized == "blackout") {
            sentinel_idle_screen(root, SentinelMode::Blackout, ansi_enabled);
        } else if (normalized == "speed" || normalized == "movie") {
            settings.sentinel_movie_speed = next_sentinel_movie_speed(
                settings.sentinel_movie_speed);
            std::string error;
            (void)save_terminal_settings(settings, error);
        } else if (normalized.rfind("speed ", 0) == 0 ||
                   normalized.rfind("movie ", 0) == 0) {
            const auto separator = normalized.find(' ');
            try {
                const auto requested = std::stod(normalized.substr(separator + 1));
                if (!valid_sentinel_movie_speed(requested)) {
                    throw std::out_of_range("sentinel movie speed");
                }
                settings.sentinel_movie_speed = requested;
                std::string error;
                if (!save_terminal_settings(settings, error)) throw std::runtime_error(error);
            } catch (...) {
                paint(ansi_enabled, ansi::red);
                std::cout << "MOVIE SPEED MUST BE 0.5, 1.0, OR 2.0\n";
                paint(ansi_enabled, ansi::reset);
                pause(ansi_enabled);
            }
        } else if (normalized == "f1" || normalized == "1" || normalized == "cards") {
            cards_screen(profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "f2" || normalized == "2" || normalized == "library") {
            if (library_ready) library_screen(library, profile, ollama_ready, ansi_enabled);
        } else if (normalized == "schematics" || normalized == "schematic" ||
                   normalized == "schem") {
            schematics_screen(root, profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "f10" || normalized == "10" ||
                   normalized == "inventory" || normalized == "inv" ||
                   normalized == "loadout") {
            inventory_screen(
                inventory_store, inventory, inventory_summary, profile, ollama_ready,
                document_count, ansi_enabled);
        } else if (normalized == "cookbook") {
            if (library_ready) cookbook_screen(library, profile, ollama_ready, ansi_enabled);
        } else if (normalized == "f3" || normalized == "3" || normalized == "guide" ||
                   normalized == "ask") {
            if (library_ready) guide_screen(library, ollama, profile, ansi_enabled);
        } else if (normalized.rfind("ask ", 0) == 0) {
            if (library_ready) guide_screen(library, ollama, profile, ansi_enabled, normalized.substr(4));
        } else if (normalized.rfind("search ", 0) == 0) {
            if (library_ready) search_screen(library, profile, ollama_ready, ansi_enabled, normalized.substr(7));
        } else if (normalized == "f4" || normalized == "4" || normalized == "profile") {
            profile_screen(profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "f5" || normalized == "5" || normalized == "status" ||
                   normalized == "system") {
            if (library_ready) system_screen(profile, ollama, library, root, settings, ansi_enabled);
        } else if (normalized == "chat" || normalized == "mesh" ||
                   normalized == "comms" || normalized == "nearby") {
            mesh_chat_screen(
                profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "io" || normalized == "i/o" ||
                   normalized == "field io" || normalized == "field-i/o") {
            if (library_ready) field_io_hub_screen(
                library, profile, ollama_ready, ansi_enabled);
        } else if (normalized == "scout" || normalized == "uart" ||
                   normalized == "uart-scout") {
            if (library_ready) io_scout_screen(
                library, profile, ollama_ready, ansi_enabled);
        } else if (normalized == "f6" || normalized == "6" || normalized == "map" ||
                   normalized == "maps") {
            map_screen(profile, ollama_ready, document_count, root, ansi_enabled);
        } else if (normalized == "f7" || normalized == "7" || normalized == "herbs" ||
                   normalized == "plants") {
            if (library_ready) herbs_screen(library, profile, ollama_ready, root, ansi_enabled);
        } else if (normalized == "f8" || normalized == "8" || normalized == "journal" ||
                   normalized == "log" || normalized == "notes") {
            journal_screen(profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "citizens" || normalized == "citizen" ||
                   normalized == "manifest") {
            citizen_registry_screen(profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "f9" || normalized == "9" || normalized == "society" ||
                   normalized == "civics" || normalized == "belief") {
            if (library_ready) society_screen(library, profile, ollama_ready, ansi_enabled);
        } else if (normalized == "f11" || normalized == "11" || normalized == "oobe" ||
                   normalized == "civilization" || normalized == "install") {
            civilization_installation_screen(
                profile, ollama_ready, document_count, root, ansi_enabled);
        } else if (normalized == "f12" || normalized == "12" || normalized == "about" ||
                   normalized == "credits") {
            about_screen(profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "help" || normalized == "?") {
            help_screen(profile, ollama_ready, document_count, ansi_enabled);
        } else if (normalized == "layout") {
            switch_display_mode(!minimal_mode);
        } else if (normalized == "clear" || normalized.empty()) {
            continue;
        } else if (normalized == "q" || normalized == "quit" || normalized == "exit" ||
                   normalized == "escape") {
            if (appliance_mode()) {
                sentinel_idle_screen(root, SentinelMode::Sentinel, ansi_enabled);
                continue;
            }
            clear(ansi_enabled);
            paint(ansi_enabled, ansi::amber);
            std::cout << "OFF-GRID TERMINAL CLOSED // LOCAL STATE SAVED\n";
            paint(ansi_enabled, ansi::reset);
            return 0;
        } else {
            // Navigation input is deliberately locked. Unknown keys are ignored
            // and never append an error line that could push the fixed screen.
            continue;
        }
        } catch (const IdleLockSignal&) {
            sentinel_idle_screen(root, SentinelMode::Sentinel, ansi_enabled);
        }
    }
}

}  // namespace offgrid

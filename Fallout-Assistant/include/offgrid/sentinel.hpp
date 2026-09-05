#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace offgrid {

constexpr int sentinel_idle_timeout_seconds = 5 * 60;
constexpr std::size_t sentinel_last_will_max_bytes = 4096;

enum class SentinelMode {
    Sentinel,
    Quiet,
    Blackout,
};

std::string_view sentinel_mode_name(SentinelMode mode);
bool valid_sentinel_movie_speed(double speed);
double next_sentinel_movie_speed(double speed);
std::chrono::milliseconds sentinel_card_duration(double speed);
std::optional<std::string> load_sentinel_last_will(
    const std::filesystem::path& path, std::string& error);
bool save_sentinel_last_will(
    const std::filesystem::path& path, std::string_view message, std::string& error);

class SentinelEscapeSequence {
public:
    // Hidden wake chord: Escape, W, K, Enter.
    bool push(std::string_view key);
    void reset();

private:
    int position_{};
};

}  // namespace offgrid

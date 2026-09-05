#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace offgrid {

enum class BootUnlock {
    None,
    Raider,
    VaultBlueRaider,
};

std::string_view boot_unlock_name(BootUnlock unlock);
std::filesystem::path boot_unlock_path();
bool schedule_boot_unlock(
    const std::filesystem::path& path, BootUnlock unlock, std::string& error);
BootUnlock consume_boot_unlock(const std::filesystem::path& path, std::string& error);

}  // namespace offgrid

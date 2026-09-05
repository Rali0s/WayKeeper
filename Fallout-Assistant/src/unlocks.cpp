#include "offgrid/unlocks.hpp"

#include "offgrid/library.hpp"

#include <cstdlib>
#include <fstream>

namespace offgrid {

std::string_view boot_unlock_name(const BootUnlock unlock) {
    switch (unlock) {
        case BootUnlock::Raider: return "raider";
        case BootUnlock::VaultBlueRaider: return "vault-blue-raider";
        case BootUnlock::None: return "none";
    }
    return "none";
}

std::filesystem::path boot_unlock_path() {
    if (const char* configured = std::getenv("OFFGRID_STATE_DIR")) {
        return std::filesystem::path(configured) / "next-boot-unlock.ini";
    }
    return resource_root() / "state" / "next-boot-unlock.ini";
}

bool schedule_boot_unlock(
    const std::filesystem::path& path, const BootUnlock unlock, std::string& error) {
    if (unlock == BootUnlock::None) {
        error = "Cannot schedule an empty boot unlock.";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(path.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create unlock state directory: " + filesystem_error.message();
        return false;
    }
    std::ofstream stream(path, std::ios::trunc);
    if (!stream) {
        error = "Could not write boot unlock state: " + path.string();
        return false;
    }
    stream << "unlock=" << boot_unlock_name(unlock) << '\n';
    if (!stream) {
        error = "Could not finish writing boot unlock state: " + path.string();
        return false;
    }
    return true;
}

BootUnlock consume_boot_unlock(const std::filesystem::path& path, std::string& error) {
    std::ifstream stream(path);
    if (!stream) return BootUnlock::None;
    std::string line;
    std::getline(stream, line);
    BootUnlock unlock = BootUnlock::None;
    if (line == "unlock=raider") unlock = BootUnlock::Raider;
    else if (line == "unlock=vault-blue-raider") unlock = BootUnlock::VaultBlueRaider;
    else error = "Unknown boot unlock state was discarded.";
    stream.close();
    std::error_code filesystem_error;
    std::filesystem::remove(path, filesystem_error);
    if (filesystem_error) {
        error = "Could not consume boot unlock state: " + filesystem_error.message();
    }
    return unlock;
}

}  // namespace offgrid

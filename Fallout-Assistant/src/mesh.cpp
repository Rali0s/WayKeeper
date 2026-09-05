#include "offgrid/mesh.hpp"

#include "offgrid/settings.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace offgrid {
namespace {

std::filesystem::path mesh_state_path(const std::string_view filename) {
    return settings_path().parent_path() / filename;
}

bool remove_state_file(const std::filesystem::path& path, std::string& error) {
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);
    if (!remove_error) return true;
    error = "Could not remove mesh state " + path.string() + ": " + remove_error.message();
    return false;
}

}  // namespace

std::filesystem::path mesh_profile_path() {
    return mesh_state_path("mesh-profile.ini");
}

std::filesystem::path mesh_history_path() {
    return mesh_state_path("mesh-history.tsv");
}

std::filesystem::path mesh_identity_path() {
    return mesh_state_path("mesh-identity.key");
}

std::filesystem::path mesh_outbox_path() {
    return mesh_state_path("mesh-outbox.bin");
}

std::string mesh_power_mode_name(const MeshPowerMode mode) {
    switch (mode) {
        case MeshPowerMode::Off: return "OFF";
        case MeshPowerMode::Eco: return "ECO";
        case MeshPowerMode::Active: return "ACTIVE";
    }
    return "OFF";
}

std::vector<std::string> mesh_sidebar_actions(const MeshProfile& profile) {
    return {
        "POWER MODE // " + mesh_power_mode_name(profile.requested_mode) + " // ENTER TO CYCLE",
        "NICKNAME // " + profile.nickname,
        "RADIO STATUS",
        "CHAT // TYPE MESSAGE AT BOTTOM INPUT",
        "BACK",
    };
}

bool parse_mesh_power_mode(const std::string_view value, MeshPowerMode& mode) {
    std::string normalized(value);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (normalized == "off") mode = MeshPowerMode::Off;
    else if (normalized == "eco") mode = MeshPowerMode::Eco;
    else if (normalized == "active" || normalized == "on") mode = MeshPowerMode::Active;
    else return false;
    return true;
}

bool valid_mesh_nickname(const std::string_view nickname) {
    if (nickname.empty() || nickname.size() > mesh_nickname_max_bytes) return false;
    return std::all_of(nickname.begin(), nickname.end(), [](const unsigned char character) {
        return std::isalnum(character) || character == '-' || character == '_';
    });
}

bool load_mesh_profile(MeshProfile& profile, std::string& error) {
    error.clear();
    std::ifstream input(mesh_profile_path());
    if (!input) return true;

    MeshProfile candidate;
    std::string line;
    while (std::getline(input, line)) {
        const auto delimiter = line.find('=');
        if (delimiter == std::string::npos) continue;
        const auto key = line.substr(0, delimiter);
        const auto value = line.substr(delimiter + 1);
        if (key == "nickname") candidate.nickname = value;
        else if (key == "channel") candidate.channel = value;
        else if (key == "mode" && !parse_mesh_power_mode(value, candidate.requested_mode)) {
            error = "Invalid mesh power mode: " + value;
            return false;
        } else if (key == "relay_while_locked") {
            candidate.relay_while_locked = value == "1";
        }
    }
    if (!valid_mesh_nickname(candidate.nickname) || candidate.channel != "#bluetooth") {
        error = "Invalid mesh profile nickname or channel.";
        return false;
    }
    profile = candidate;
    return true;
}

bool save_mesh_profile(const MeshProfile& profile, std::string& error) {
    error.clear();
    if (!valid_mesh_nickname(profile.nickname) || profile.channel != "#bluetooth") {
        error = "Mesh nickname must be 1-24 letters, digits, '-' or '_'.";
        return false;
    }
    std::error_code directory_error;
    std::filesystem::create_directories(mesh_profile_path().parent_path(), directory_error);
    if (directory_error) {
        error = "Could not create mesh state directory: " + directory_error.message();
        return false;
    }
    std::ofstream output(mesh_profile_path(), std::ios::trunc);
    if (!output) {
        error = "Could not write mesh profile: " + mesh_profile_path().string();
        return false;
    }
    output << "nickname=" << profile.nickname << '\n'
           << "channel=" << profile.channel << '\n'
           << "mode=" << mesh_power_mode_name(profile.requested_mode) << '\n'
           << "relay_while_locked=" << (profile.relay_while_locked ? 1 : 0) << '\n';
    return static_cast<bool>(output);
}

MeshReadiness inspect_mesh_readiness() {
    MeshReadiness readiness;
#ifdef __linux__
    readiness.linux_host = true;
    readiness.bluez_runtime =
        std::filesystem::exists("/run/dbus/system_bus_socket") &&
        (std::filesystem::exists("/usr/lib/bluetooth/bluetoothd") ||
         std::filesystem::exists("/usr/libexec/bluetooth/bluetoothd") ||
         std::filesystem::exists("/usr/sbin/bluetoothd"));
    const std::filesystem::path bluetooth_class{"/sys/class/bluetooth"};
    std::error_code iterator_error;
    if (std::filesystem::is_directory(bluetooth_class, iterator_error)) {
        for (const auto& entry : std::filesystem::directory_iterator(
                 bluetooth_class, iterator_error)) {
            if (entry.path().filename().string().rfind("hci", 0) != 0) continue;
            readiness.controller_found = true;
            readiness.controller = entry.path().filename().string();
            break;
        }
    }
    readiness.detail = !readiness.bluez_runtime
        ? "BlueZ runtime or system D-Bus was not detected."
        : !readiness.controller_found
            ? "BlueZ is present but no hci controller was detected."
            : "Controller detected; simultaneous GATT central/peripheral and BitChat interoperability remain unqualified.";
#elif defined(__APPLE__)
    readiness.detail =
        "macOS development host: ANSI integration is available, but the Linux BlueZ radio backend is not built here.";
#elif defined(_WIN32)
    readiness.detail =
        "Windows development host: ANSI integration is available, but the Linux BlueZ radio backend is not built here.";
#else
    readiness.detail = "Unsupported host for the planned Linux BlueZ radio backend.";
#endif
    // Deliberately locked until the BlueZ GATT transport, protocol vectors, and
    // three-device interoperability gate described in the workflow are complete.
    readiness.protocol_backend_ready = false;
    return readiness;
}

bool clear_mesh_history(std::string& error) {
    error.clear();
    return remove_state_file(mesh_history_path(), error);
}

bool wipe_mesh_state(std::string& error) {
    error.clear();
    for (const auto& path : {
             mesh_profile_path(), mesh_history_path(), mesh_identity_path(),
             mesh_outbox_path()}) {
        if (!remove_state_file(path, error)) return false;
    }
    return true;
}

}  // namespace offgrid

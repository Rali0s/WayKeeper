#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

enum class MeshPowerMode {
    Off,
    Eco,
    Active,
};

struct MeshProfile {
    std::string nickname{"WAYKEEPER"};
    std::string channel{"#bluetooth"};
    MeshPowerMode requested_mode{MeshPowerMode::Off};
    bool relay_while_locked{};
};

struct MeshReadiness {
    bool linux_host{};
    bool bluez_runtime{};
    bool controller_found{};
    bool protocol_backend_ready{};
    std::string controller{"NONE"};
    std::string detail;
};

constexpr std::size_t mesh_nickname_max_bytes = 24;
constexpr std::size_t mesh_message_max_bytes = 512;

std::filesystem::path mesh_profile_path();
std::filesystem::path mesh_history_path();
std::filesystem::path mesh_identity_path();
std::filesystem::path mesh_outbox_path();

std::string mesh_power_mode_name(MeshPowerMode mode);
std::vector<std::string> mesh_sidebar_actions(const MeshProfile& profile);
bool parse_mesh_power_mode(std::string_view value, MeshPowerMode& mode);
bool valid_mesh_nickname(std::string_view nickname);
bool load_mesh_profile(MeshProfile& profile, std::string& error);
bool save_mesh_profile(const MeshProfile& profile, std::string& error);
MeshReadiness inspect_mesh_readiness();
bool clear_mesh_history(std::string& error);
bool wipe_mesh_state(std::string& error);

}  // namespace offgrid

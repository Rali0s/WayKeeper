#include "offgrid/profile.hpp"

#include "offgrid/library.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string_view>

namespace offgrid {
namespace {

const std::vector<std::string> incidents{
    "Test / Simulation",
    "Nuclear / Radiological",
    "Famine / Food Crisis",
    "Zombie / Fictional Drill",
    "Outbreak / Epidemic",
    "Flood",
    "Wildfire",
    "Earthquake",
    "Severe Weather",
    "Grid / Infrastructure Failure",
    "Other / General Emergency"
};

const std::vector<std::string> terrains{
    "Urban",
    "Suburban",
    "Rural / Farmland",
    "Forest / Woodland",
    "Mountain / Alpine",
    "Desert / Arid",
    "Tundra / Arctic",
    "Coastal / Marine",
    "Wetlands / Swamp",
    "Other / Mixed Terrain"
};

std::string trim(std::string value) {
    const auto not_space = [](const unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string clean_value(std::string value, const std::size_t limit = 64) {
    value = trim(std::move(value));
    value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char c) {
        return c < 0x20 || c == 0x7f;
    }), value.end());
    if (value.size() > limit) value.resize(limit);
    return value;
}

}  // namespace

bool OperatorProfile::valid() const {
    return !name.empty() && !incident.empty() && !terrain.empty();
}

bool OperatorProfile::simulation() const {
    return incident.find("Test") != std::string::npos ||
           incident.find("Fictional") != std::string::npos;
}

const std::vector<std::string>& incident_options() { return incidents; }
const std::vector<std::string>& terrain_options() { return terrains; }

std::filesystem::path profile_path() {
    if (const char* configured = std::getenv("OFFGRID_STATE_DIR")) {
        return std::filesystem::path(configured) / "profile.ini";
    }
    return resource_root() / "state" / "profile.ini";
}

bool load_profile(OperatorProfile& profile, std::string& error) {
    std::ifstream stream(profile_path());
    if (!stream) {
        error = "No local operator profile exists yet.";
        return false;
    }

    OperatorProfile loaded;
    std::string line;
    while (std::getline(stream, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) continue;
        const auto key = line.substr(0, separator);
        const auto value = clean_value(line.substr(separator + 1));
        if (key == "name") loaded.name = value;
        else if (key == "incident") loaded.incident = value;
        else if (key == "terrain") loaded.terrain = value;
    }
    if (!loaded.valid()) {
        error = "The local profile is incomplete. Run onboarding again.";
        return false;
    }
    profile = std::move(loaded);
    return true;
}

bool save_profile(const OperatorProfile& profile, std::string& error) {
    OperatorProfile clean{
        clean_value(profile.name, 32), clean_value(profile.incident), clean_value(profile.terrain)};
    if (!clean.valid()) {
        error = "Name, incident, and terrain are required.";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(profile_path().parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create local state directory: " + filesystem_error.message();
        return false;
    }
    std::ofstream stream(profile_path(), std::ios::trunc);
    if (!stream) {
        error = "Could not write local profile: " + profile_path().string();
        return false;
    }
    stream << "name=" << clean.name << '\n'
           << "incident=" << clean.incident << '\n'
           << "terrain=" << clean.terrain << '\n';
    return static_cast<bool>(stream);
}

}  // namespace offgrid


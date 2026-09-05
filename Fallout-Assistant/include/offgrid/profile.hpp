#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace offgrid {

struct OperatorProfile {
    std::string name;
    std::string incident;
    std::string terrain;

    bool valid() const;
    bool simulation() const;
};

const std::vector<std::string>& incident_options();
const std::vector<std::string>& terrain_options();
std::filesystem::path profile_path();
bool load_profile(OperatorProfile& profile, std::string& error);
bool save_profile(const OperatorProfile& profile, std::string& error);

}  // namespace offgrid


#pragma once

#include <filesystem>
#include <string>

namespace offgrid {

bool java_companion_support_available();
bool launch_java_companion(
    const std::filesystem::path& resource_root,
    const std::filesystem::path& profile_file,
    const std::filesystem::path& settings_file,
    std::string& error);

}  // namespace offgrid

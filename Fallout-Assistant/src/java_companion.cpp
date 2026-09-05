#include "offgrid/java_companion.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace offgrid {
namespace {

std::string shell_quote(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (const char character : value) {
        if (character == '"') quoted += "\\\"";
        else quoted += character;
    }
    return quoted + '"';
#else
    std::string quoted = "'";
    for (const char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + '\'';
#endif
}

long current_process_id() {
#ifdef _WIN32
    return static_cast<long>(GetCurrentProcessId());
#else
    return static_cast<long>(getpid());
#endif
}

bool disabled_by_environment() {
    const char* configured = std::getenv("OFFGRID_JAVA_COMPANION");
    return configured && (std::string(configured) == "0" ||
                          std::string(configured) == "off" ||
                          std::string(configured) == "false");
}

}  // namespace

bool java_companion_support_available() {
#if defined(OFFGRID_JAVA_COMPANION_JAR) && defined(OFFGRID_JAVA_EXECUTABLE)
    return !disabled_by_environment() &&
           std::filesystem::exists(OFFGRID_JAVA_COMPANION_JAR) &&
           std::filesystem::exists(OFFGRID_JAVA_EXECUTABLE);
#else
    return false;
#endif
}

bool launch_java_companion(
    const std::filesystem::path& resource_root,
    const std::filesystem::path& profile_file,
    const std::filesystem::path& settings_file,
    std::string& error) {
#if !defined(OFFGRID_JAVA_COMPANION_JAR) || !defined(OFFGRID_JAVA_EXECUTABLE)
    (void)resource_root;
    (void)profile_file;
    (void)settings_file;
    error = "Java companion was not included in this build.";
    return false;
#else
    if (!java_companion_support_available()) {
        error = "Java companion runtime or JAR is unavailable.";
        return false;
    }

    const std::string arguments =
        " -jar " + shell_quote(OFFGRID_JAVA_COMPANION_JAR) +
        " --root " + shell_quote(resource_root.string()) +
        " --profile " + shell_quote(profile_file.string()) +
        " --settings " + shell_quote(settings_file.string()) +
        " --parent " + std::to_string(current_process_id());
#ifdef _WIN32
    const std::string command =
        "start \"\" /B " + shell_quote(OFFGRID_JAVA_EXECUTABLE) + arguments +
        " >NUL 2>NUL";
#else
    const std::string command =
        shell_quote(OFFGRID_JAVA_EXECUTABLE) + arguments + " >/dev/null 2>&1 &";
#endif
    if (std::system(command.c_str()) != 0) {
        error = "Could not start the Java full-fidelity companion.";
        return false;
    }
    return true;
#endif
}

}  // namespace offgrid

#include "offgrid/journal.hpp"

#include "offgrid/profile.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace offgrid {
namespace {

constexpr std::string_view magic = "OFFGRID-JOURNAL-1";

std::string clean_header(std::string value, const std::size_t limit = 160) {
    value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char character) {
        return character == '\n' || character == '\r' || character == '\0' ||
               (character < 0x20 && character != '\t');
    }), value.end());
    std::replace(value.begin(), value.end(), '\t', ' ');
    if (value.size() > limit) value.resize(limit);
    return value;
}

std::string local_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S %z");
    return output.str();
}

std::string generate_id(const std::filesystem::path& root) {
    const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto base = std::to_string(milliseconds);
    std::string candidate = base;
    for (std::size_t suffix = 1;
         std::filesystem::exists(root / (candidate + ".log")) ||
         std::filesystem::exists(root / "archive" / (candidate + ".log")); ++suffix) {
        candidate = base + "-" + std::to_string(suffix);
    }
    return candidate;
}

std::filesystem::path entry_path(
    const std::filesystem::path& root, const std::string_view id, const bool archived) {
    const auto directory = archived ? root / "archive" : root;
    return directory / (std::string(id) + ".log");
}

bool valid_id(const std::string_view id) {
    return !id.empty() && std::all_of(id.begin(), id.end(), [](const unsigned char character) {
        return std::isdigit(character) || character == '-';
    });
}

std::optional<JournalEntry> read_entry(const std::filesystem::path& path, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not read journal entry: " + path.string();
        return std::nullopt;
    }
    std::string line;
    if (!std::getline(input, line) || line != magic) {
        error = "Unsupported journal entry format: " + path.string();
        return std::nullopt;
    }
    JournalEntry entry;
    bool body_found = false;
    while (std::getline(input, line)) {
        if (line == "---") {
            body_found = true;
            break;
        }
        const auto delimiter = line.find('=');
        if (delimiter == std::string::npos) continue;
        const auto key = line.substr(0, delimiter);
        const auto value = line.substr(delimiter + 1);
        if (key == "id") entry.id = value;
        else if (key == "created") entry.created_at = value;
        else if (key == "updated") entry.updated_at = value;
        else if (key == "kind") entry.kind = value;
        else if (key == "title") entry.title = value;
        else if (key == "tags") entry.tags = value;
        else if (key == "operator") entry.operator_name = value;
        else if (key == "incident") entry.incident = value;
        else if (key == "terrain") entry.terrain = value;
        else if (key == "sleep_hours") {
            try { entry.sleep_hours = std::stod(value); } catch (...) {}
        }
        else if (key == "miles_traveled") {
            try { entry.miles_traveled = std::stod(value); } catch (...) {}
        }
        else if (key == "health_note") entry.health_note = value;
    }
    if (body_found) {
        std::ostringstream body;
        body << input.rdbuf();
        entry.body = body.str();
        if (!entry.body.empty() && entry.body.back() == '\n') entry.body.pop_back();
    }
    if (!valid_id(entry.id) || entry.title.empty() || path.stem().string() != entry.id) {
        error = "Journal entry is incomplete: " + path.string();
        return std::nullopt;
    }
    return entry;
}

bool write_entry(
    const std::filesystem::path& root, const JournalEntry& entry, std::string& error) {
    if (!valid_id(entry.id) || entry.title.empty()) {
        error = "Journal entries require a valid ID and title.";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(root, filesystem_error);
    if (filesystem_error) {
        error = "Could not create journal directory: " + filesystem_error.message();
        return false;
    }
    const auto destination = entry_path(root, entry.id, false);
    auto temporary = destination;
    temporary += ".tmp";
    auto backup = destination;
    backup += ".bak";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Could not write temporary journal entry: " + temporary.string();
            return false;
        }
        output << magic << '\n'
               << "id=" << entry.id << '\n'
               << "created=" << clean_header(entry.created_at) << '\n'
               << "updated=" << clean_header(entry.updated_at) << '\n'
               << "kind=" << clean_header(entry.kind, 40) << '\n'
               << "title=" << clean_header(entry.title) << '\n'
               << "tags=" << clean_header(entry.tags) << '\n'
               << "operator=" << clean_header(entry.operator_name, 64) << '\n'
               << "incident=" << clean_header(entry.incident, 80) << '\n'
               << "terrain=" << clean_header(entry.terrain, 80) << '\n'
               << "sleep_hours=" << entry.sleep_hours << '\n'
               << "miles_traveled=" << entry.miles_traveled << '\n'
               << "health_note=" << clean_header(entry.health_note, 160) << '\n'
               << "---\n" << entry.body << '\n';
        if (!output) {
            error = "Could not finish writing journal entry: " + temporary.string();
            return false;
        }
    }

    const bool replacing = std::filesystem::exists(destination);
    if (replacing) {
        std::filesystem::remove(backup, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(destination, backup, filesystem_error);
        if (filesystem_error) {
            std::filesystem::remove(temporary);
            error = "Could not protect the existing journal entry: " + filesystem_error.message();
            return false;
        }
    }
    filesystem_error.clear();
    std::filesystem::rename(temporary, destination, filesystem_error);
    if (filesystem_error) {
        if (replacing) {
            std::error_code restore_error;
            std::filesystem::rename(backup, destination, restore_error);
        }
        error = "Could not install journal entry: " + filesystem_error.message();
        return false;
    }
    if (replacing) std::filesystem::remove(backup, filesystem_error);
    return true;
}

std::vector<JournalEntry> load_directory(
    const std::filesystem::path& directory, std::string& error) {
    std::vector<JournalEntry> results;
    std::error_code filesystem_error;
    if (!std::filesystem::exists(directory, filesystem_error)) return results;
    for (const auto& item : std::filesystem::directory_iterator(directory, filesystem_error)) {
        if (filesystem_error) break;
        if (!item.is_regular_file() || item.path().extension() != ".log") continue;
        std::string entry_error;
        const auto entry = read_entry(item.path(), entry_error);
        if (entry) results.push_back(*entry);
        else if (error.empty()) error = entry_error;
    }
    if (filesystem_error && error.empty()) {
        error = "Could not scan journal directory: " + filesystem_error.message();
    }
    std::sort(results.begin(), results.end(), [](const JournalEntry& left, const JournalEntry& right) {
        if (left.updated_at != right.updated_at) return left.updated_at > right.updated_at;
        return left.id > right.id;
    });
    return results;
}

bool move_entry(
    const std::filesystem::path& source, const std::filesystem::path& destination,
    std::string& error) {
    std::error_code filesystem_error;
    std::filesystem::create_directories(destination.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create journal archive: " + filesystem_error.message();
        return false;
    }
    if (!std::filesystem::exists(source)) {
        error = "Journal entry not found: " + source.string();
        return false;
    }
    if (std::filesystem::exists(destination)) {
        error = "Journal destination already exists: " + destination.string();
        return false;
    }
    std::filesystem::rename(source, destination, filesystem_error);
    if (!filesystem_error) return true;
    filesystem_error.clear();
    std::filesystem::copy_file(source, destination, filesystem_error);
    if (filesystem_error) {
        error = "Could not move journal entry: " + filesystem_error.message();
        return false;
    }
    std::filesystem::remove(source, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(destination);
        error = "Could not complete journal move: " + filesystem_error.message();
        return false;
    }
    return true;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

}  // namespace

JournalStore::JournalStore(std::filesystem::path root) : root_(std::move(root)) {}

const std::filesystem::path& JournalStore::root() const { return root_; }

std::vector<JournalEntry> JournalStore::entries(std::string& error) const {
    return load_directory(root_, error);
}

std::vector<JournalEntry> JournalStore::archived_entries(std::string& error) const {
    return load_directory(root_ / "archive", error);
}

std::vector<JournalEntry> JournalStore::search(
    const std::string_view query, std::string& error) const {
    const auto needle = lower(std::string(query));
    if (needle.empty()) return entries(error);
    auto all = entries(error);
    all.erase(std::remove_if(all.begin(), all.end(), [&](const JournalEntry& entry) {
        const auto haystack = lower(
            entry.title + "\n" + entry.tags + "\n" + entry.kind + "\n" + entry.incident +
            "\n" + entry.terrain + "\n" + entry.health_note + "\n" + entry.body);
        return haystack.find(needle) == std::string::npos;
    }), all.end());
    return all;
}

std::optional<JournalEntry> JournalStore::find(
    const std::string_view id, const bool archived, std::string& error) const {
    if (!valid_id(id)) {
        error = "Invalid journal entry ID.";
        return std::nullopt;
    }
    return read_entry(entry_path(root_, id, archived), error);
}

bool JournalStore::create(JournalEntry& entry, std::string& error) const {
    if (entry.title.empty() || entry.body.empty()) {
        error = "A journal title and body are required.";
        return false;
    }
    entry.id = generate_id(root_);
    entry.created_at = local_timestamp();
    entry.updated_at = entry.created_at;
    if (entry.kind.empty()) entry.kind = "CAPTAINS_LOG";
    return write_entry(root_, entry, error);
}

bool JournalStore::update(JournalEntry& entry, std::string& error) const {
    if (!valid_id(entry.id) || entry.title.empty() || entry.body.empty()) {
        error = "A journal ID, title, and body are required.";
        return false;
    }
    entry.updated_at = local_timestamp();
    return write_entry(root_, entry, error);
}

bool JournalStore::archive(const std::string_view id, std::string& error) const {
    if (!valid_id(id)) {
        error = "Invalid journal entry ID.";
        return false;
    }
    return move_entry(entry_path(root_, id, false), entry_path(root_, id, true), error);
}

bool JournalStore::restore(const std::string_view id, std::string& error) const {
    if (!valid_id(id)) {
        error = "Invalid journal entry ID.";
        return false;
    }
    return move_entry(entry_path(root_, id, true), entry_path(root_, id, false), error);
}

std::filesystem::path journal_path() {
    return profile_path().parent_path() / "journal";
}

}  // namespace offgrid

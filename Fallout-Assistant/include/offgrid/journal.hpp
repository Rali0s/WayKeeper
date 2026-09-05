#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

struct JournalEntry {
    std::string id;
    std::string created_at;
    std::string updated_at;
    std::string kind;
    std::string title;
    std::string tags;
    std::string operator_name;
    std::string incident;
    std::string terrain;
    double sleep_hours{};
    double miles_traveled{};
    std::string health_note;
    std::string body;
};

class JournalStore {
public:
    explicit JournalStore(std::filesystem::path root);

    const std::filesystem::path& root() const;
    std::vector<JournalEntry> entries(std::string& error) const;
    std::vector<JournalEntry> archived_entries(std::string& error) const;
    std::vector<JournalEntry> search(std::string_view query, std::string& error) const;
    std::optional<JournalEntry> find(std::string_view id, bool archived, std::string& error) const;
    bool create(JournalEntry& entry, std::string& error) const;
    bool update(JournalEntry& entry, std::string& error) const;
    bool archive(std::string_view id, std::string& error) const;
    bool restore(std::string_view id, std::string& error) const;

private:
    std::filesystem::path root_;
};

std::filesystem::path journal_path();

}  // namespace offgrid

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace offgrid {

struct HerbDatabaseStats {
    std::size_t documents{};
    std::size_t pages{};
    std::size_t structured_plants{};
    std::size_t reviewed_statements{};
};

struct HerbSearchHit {
    std::string document_id;
    std::string title;
    std::size_t page{};
    std::size_t page_count{};
    std::string excerpt;
    std::filesystem::path pdf_path;
};

struct HerbPage {
    std::string document_id;
    std::string title;
    std::size_t page{};
    std::size_t page_count{};
    std::string text;
    std::filesystem::path pdf_path;
};

bool herb_database_support_available();
std::filesystem::path herb_database_path(const std::filesystem::path& resource_root);
bool inspect_herb_database(
    const std::filesystem::path& database, HerbDatabaseStats& stats, std::string& error);
std::vector<HerbSearchHit> search_herb_database(
    const std::filesystem::path& database,
    const std::filesystem::path& resource_root,
    const std::string& query,
    std::size_t limit,
    std::string& error);
std::optional<HerbPage> read_herb_page(
    const std::filesystem::path& database,
    const std::filesystem::path& resource_root,
    const std::string& document_id,
    std::size_t page,
    std::string& error);

}  // namespace offgrid

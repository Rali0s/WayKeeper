#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

struct LibraryDocument {
    std::string id;
    std::string title;
    std::string category;
    std::size_t pages{};
    std::filesystem::path pdf_path;
    std::filesystem::path text_path;
};

struct SearchHit {
    std::size_t document_index{};
    std::size_t page{};
    std::string snippet;
};

class SurvivalLibrary {
public:
    bool load(const std::filesystem::path& catalog_path, std::string& error);
    const std::vector<LibraryDocument>& documents() const;
    std::vector<SearchHit> search(std::string_view query, std::size_t limit = 12) const;
    std::vector<SearchHit> search_document(
        std::size_t document_index, std::string_view query,
        std::size_t limit = 24) const;
    std::optional<std::string> read_page(std::size_t document_index, std::size_t page) const;

private:
    std::vector<LibraryDocument> documents_;
    // Lazily cache source text so repeated evidence searches do not reread a large
    // offline corpus from storage and waste battery.
    mutable std::vector<std::optional<std::string>> text_cache_;
};

std::filesystem::path resource_root();

}  // namespace offgrid

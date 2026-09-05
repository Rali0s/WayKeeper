#include "offgrid/library.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>

namespace offgrid {
namespace {

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const auto end = line.find('\t', start);
        fields.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return fields;
}

std::string lower(std::string_view input) {
    std::string output(input);
    std::transform(output.begin(), output.end(), output.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return output;
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return std::nullopt;
    std::ostringstream contents;
    contents << stream.rdbuf();
    return contents.str();
}

std::string clean_snippet(std::string_view text, std::size_t position) {
    constexpr std::size_t radius = 170;
    const std::size_t begin = position > radius ? position - radius : 0;
    const std::size_t count = std::min(text.size() - begin, radius * 2);
    std::string snippet(text.substr(begin, count));
    for (char& c : snippet) {
        if (c == '\n' || c == '\r' || c == '\f' || c == '\t') c = ' ';
    }
    std::string compact;
    compact.reserve(snippet.size());
    bool space = false;
    for (const char c : snippet) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!space) compact.push_back(' ');
            space = true;
        } else {
            compact.push_back(c);
            space = false;
        }
    }
    return compact;
}

std::vector<std::string> search_terms(const std::string_view query) {
    static const std::set<std::string> ignored{
        "and", "are", "can", "for", "from", "how", "into", "should", "the", "this",
        "what", "when", "where", "which", "with", "would", "your"};
    std::string normalized = lower(query);
    for (char& c : normalized) {
        if (!std::isalnum(static_cast<unsigned char>(c))) c = ' ';
    }
    std::istringstream words(normalized);
    std::set<std::string> unique;
    std::string word;
    while (words >> word) {
        if (word.size() >= 3 && !ignored.contains(word)) unique.insert(word);
    }
    return {unique.begin(), unique.end()};
}

}  // namespace

bool SurvivalLibrary::load(const std::filesystem::path& catalog_path, std::string& error) {
    documents_.clear();
    text_cache_.clear();
    std::ifstream catalog(catalog_path);
    if (!catalog) {
        error = "Library catalog not found: " + catalog_path.string();
        return false;
    }

    const auto base = catalog_path.parent_path();
    std::string line;
    bool first = true;
    while (std::getline(catalog, line)) {
        if (first) { first = false; continue; }
        if (line.empty()) continue;
        const auto fields = split_tabs(line);
        if (fields.size() != 6) continue;
        try {
            documents_.push_back({
                fields[0], fields[1], fields[2], static_cast<std::size_t>(std::stoull(fields[3])),
                base / fields[4], base / fields[5]});
        } catch (...) {
            error = "Invalid catalog entry: " + line;
            documents_.clear();
            return false;
        }
    }

    if (documents_.empty()) {
        error = "The library catalog contains no documents.";
        return false;
    }
    text_cache_.resize(documents_.size());
    return true;
}

const std::vector<LibraryDocument>& SurvivalLibrary::documents() const { return documents_; }

std::vector<SearchHit> SurvivalLibrary::search(
    const std::string_view query, const std::size_t limit) const {
    struct RankedHit {
        SearchHit hit;
        std::size_t score{};
    };
    std::vector<RankedHit> ranked;
    const std::string phrase = lower(query);
    const auto terms = search_terms(query);
    if (phrase.empty() || terms.empty()) return {};

    for (std::size_t index = 0; index < documents_.size(); ++index) {
        if (documents_[index].category.starts_with("Cookbook-Underground-Restricted") ||
            documents_[index].category.starts_with("Archive-")) {
            continue;
        }
        if (!text_cache_[index]) text_cache_[index] = read_file(documents_[index].text_path);
        const auto& text = text_cache_[index];
        if (!text) continue;

        std::size_t page_start = 0;
        std::size_t page_number = 1;
        while (page_start <= text->size()) {
            const auto boundary = text->find('\f', page_start);
            const auto count = boundary == std::string::npos ? text->size() - page_start : boundary - page_start;
            const std::string_view page_text(text->data() + page_start, count);
            const std::string searchable = lower(page_text);
            std::size_t score = 0;
            std::size_t first_match = std::string::npos;
            const auto phrase_match = searchable.find(phrase);
            if (phrase_match != std::string::npos) {
                score += 20;
                first_match = phrase_match;
            }
            for (const auto& term : terms) {
                std::size_t cursor = 0;
                std::size_t occurrences = 0;
                while ((cursor = searchable.find(term, cursor)) != std::string::npos && occurrences < 5) {
                    if (first_match == std::string::npos) first_match = cursor;
                    ++occurrences;
                    cursor += term.size();
                }
                score += occurrences;
            }
            if (score > 0 && first_match != std::string::npos) {
                ranked.push_back({{index, page_number, clean_snippet(page_text, first_match)}, score});
            }
            if (boundary == std::string::npos) break;
            page_start = boundary + 1;
            ++page_number;
        }
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedHit& left, const RankedHit& right) {
        return left.score > right.score;
    });
    std::vector<SearchHit> results;
    for (std::size_t index = 0; index < std::min(limit, ranked.size()); ++index) {
        results.push_back(std::move(ranked[index].hit));
    }
    return results;
}

std::vector<SearchHit> SurvivalLibrary::search_document(
    const std::size_t document_index, const std::string_view query,
    const std::size_t limit) const {
    struct RankedHit {
        SearchHit hit;
        std::size_t score{};
    };
    std::vector<RankedHit> ranked;
    if (document_index >= documents_.size()) return {};
    const std::string phrase = lower(query);
    const auto terms = search_terms(query);
    if (phrase.empty() || terms.empty()) return {};
    if (!text_cache_[document_index]) {
        text_cache_[document_index] = read_file(documents_[document_index].text_path);
    }
    const auto& text = text_cache_[document_index];
    if (!text) return {};

    std::size_t page_start = 0;
    std::size_t page_number = 1;
    while (page_start <= text->size()) {
        const auto boundary = text->find('\f', page_start);
        const auto count = boundary == std::string::npos
            ? text->size() - page_start : boundary - page_start;
        const std::string_view page_text(text->data() + page_start, count);
        const std::string searchable = lower(page_text);
        std::size_t score = 0;
        std::size_t first_match = std::string::npos;
        const auto phrase_match = searchable.find(phrase);
        if (phrase_match != std::string::npos) {
            score += 20;
            first_match = phrase_match;
        }
        for (const auto& term : terms) {
            std::size_t cursor = 0;
            std::size_t occurrences = 0;
            while ((cursor = searchable.find(term, cursor)) != std::string::npos &&
                   occurrences < 5) {
                if (first_match == std::string::npos) first_match = cursor;
                ++occurrences;
                cursor += term.size();
            }
            score += occurrences;
        }
        if (score > 0 && first_match != std::string::npos) {
            ranked.push_back({
                {document_index, page_number, clean_snippet(page_text, first_match)}, score});
        }
        if (boundary == std::string::npos) break;
        page_start = boundary + 1;
        ++page_number;
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
        return left.score > right.score;
    });
    std::vector<SearchHit> results;
    for (std::size_t index = 0; index < std::min(limit, ranked.size()); ++index) {
        results.push_back(std::move(ranked[index].hit));
    }
    return results;
}

std::optional<std::string> SurvivalLibrary::read_page(
    const std::size_t document_index, const std::size_t page) const {
    if (document_index >= documents_.size() || page == 0) return std::nullopt;
    if (!text_cache_[document_index]) {
        text_cache_[document_index] = read_file(documents_[document_index].text_path);
    }
    const auto& text = text_cache_[document_index];
    if (!text) return std::nullopt;

    std::size_t start = 0;
    for (std::size_t current = 1; current < page; ++current) {
        const auto boundary = text->find('\f', start);
        if (boundary == std::string::npos) return std::nullopt;
        start = boundary + 1;
    }
    const auto end = text->find('\f', start);
    return text->substr(start, end == std::string::npos ? end : end - start);
}

std::filesystem::path resource_root() {
    if (const char* configured = std::getenv("OFFGRID_HOME")) {
        return std::filesystem::path(configured);
    }
    const auto current = std::filesystem::current_path();
    if (std::filesystem::exists(current / "library" / "catalog.tsv")) return current;
#ifdef OFFGRID_SOURCE_DIR
    return std::filesystem::path(OFFGRID_SOURCE_DIR);
#else
    return current;
#endif
}

}  // namespace offgrid

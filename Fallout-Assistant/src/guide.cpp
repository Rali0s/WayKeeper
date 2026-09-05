#include "offgrid/guide.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>

namespace offgrid {
namespace {

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

bool contains(const std::string& text, const std::string_view needle) {
    return text.find(needle) != std::string::npos;
}

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](const unsigned char c) {
        return std::isspace(c) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](const unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

bool parse_candidate(
    const std::filesystem::path& path, CandidateCard& card, std::string& error) {
    std::ifstream stream(path);
    if (!stream) {
        error = "Could not open candidate card: " + path.string();
        return false;
    }
    const std::string text{
        std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
    if (!text.starts_with("---\n")) {
        error = "Candidate card has no front matter: " + path.string();
        return false;
    }
    const auto metadata_end = text.find("\n---\n", 4);
    if (metadata_end == std::string::npos) {
        error = "Candidate card has unterminated front matter: " + path.string();
        return false;
    }

    std::map<std::string, std::string> metadata;
    std::istringstream metadata_stream(text.substr(4, metadata_end - 4));
    for (std::string line; std::getline(metadata_stream, line);) {
        const auto separator = line.find(':');
        if (separator == std::string::npos) continue;
        metadata[trim(line.substr(0, separator))] = trim(line.substr(separator + 1));
    }
    const auto value = [&](const std::string& key) -> std::string {
        const auto found = metadata.find(key);
        return found == metadata.end() ? std::string{} : found->second;
    };
    if (value("schema") != "waykeeper-card-v1" || value("status") != "candidate") {
        error = "Review queue accepts only waykeeper-card-v1 candidates: " + path.string();
        return false;
    }

    const std::string body = trim(text.substr(metadata_end + 5));
    const auto heading_end = body.find('\n');
    const auto limits_heading = body.find("\n## LIMITS\n");
    const auto source_heading = body.find("\n## SOURCE NOTE\n");
    if (!body.starts_with("# ") || heading_end == std::string::npos ||
        limits_heading == std::string::npos || source_heading == std::string::npos ||
        source_heading <= limits_heading) {
        error = "Candidate card is missing required ANSI sections: " + path.string();
        return false;
    }

    card.id = value("id");
    card.title = value("title");
    card.risk = value("risk");
    card.trust = value("trust");
    card.answer = trim(body.substr(heading_end + 1, limits_heading - heading_end - 1));
    const auto limits_start = limits_heading + std::string("\n## LIMITS\n").size();
    card.limits = trim(body.substr(limits_start, source_heading - limits_start));
    const auto source_start = source_heading + std::string("\n## SOURCE NOTE\n").size();
    card.source_note = trim(body.substr(source_start));
    card.source_document = value("source_doc_id");
    card.source_pdf = value("source_pdf");
    card.source_pages = value("source_pages");
    card.source_published = value("source_published");
    card.cross_check_url = value("cross_check_url");
    card.cross_checked = value("cross_checked");

    if (card.id.empty() || card.title.empty() || card.risk.empty() || card.answer.empty() ||
        card.limits.empty() || card.source_pdf.empty() || card.source_pages.empty() ||
        card.cross_check_url.empty() || card.cross_checked.empty()) {
        error = "Candidate card is missing required review data: " + path.string();
        return false;
    }
    return true;
}

const std::vector<GuideCard> cards{
    {
        "water.tablets.contact-time",
        "Water-purification tablet contact time",
        "Use the exact contact time and dose printed on the tablet package. There is no safe "
        "universal tablet time: the active ingredient, dose, water volume, temperature, clarity, "
        "and target organism matter. Do not identify an unknown tablet by shape or color. "
        "Chlorine or iodine may not reliably kill Cryptosporidium; correctly used chlorine dioxide "
        "can require several hours. Tablets also cannot make water contaminated with fuel, toxic "
        "chemicals, or radioactive material safe. Give me the brand, active ingredient, tablet "
        "strength, water volume, and label directions for a product-specific check.",
        "https://www.cdc.gov/water-emergency/about/index.html",
        "2026-08-15"
    },
    {
        "water.boiling",
        "Making water safer by boiling",
        "If safe bottled water is unavailable and the concern is germs, filter cloudy water through "
        "a clean cloth or let it settle, then bring the clear water to a rolling boil for 1 minute. "
        "Above 6,500 feet (about 1,980 m), boil for 3 minutes. Let it cool and store it in clean, "
        "covered containers. Boiling does not remove fuel, toxic chemicals, or radioactive material; "
        "use another water source when those contaminants are known or suspected.",
        "https://www.cdc.gov/water-emergency/about/index.html",
        "2026-08-15"
    },
    {
        "radiation.shelter",
        "Immediate radiation-emergency shelter",
        "Get inside, stay inside, and stay tuned to emergency officials. Prefer a basement or the "
        "middle of a substantial building, away from exterior walls, doors, windows, and the roof. "
        "Close and lock windows and doors. If you were outside, remove the outer layer of clothing "
        "carefully and wash exposed skin if possible. Do not leave shelter until officials say it is "
        "safe unless the building is unstable or you have a life-threatening condition.",
        "https://www.cdc.gov/radiation-emergencies/response/index.html",
        "2026-08-15"
    }
};

}  // namespace

std::optional<GuideCard> find_reviewed_card(const std::string_view question) {
    const std::string query = lowercase(question);
    if (contains(query, "water") && (contains(query, "tablet") || contains(query, "purif"))) {
        return cards[0];
    }
    if (contains(query, "water") && (contains(query, "boil") || contains(query, "germ"))) {
        return cards[1];
    }
    if (contains(query, "radiation") || contains(query, "fallout") || contains(query, "nuclear")) {
        return cards[2];
    }
    return std::nullopt;
}

const std::vector<GuideCard>& reviewed_cards() { return cards; }

std::vector<CandidateCard> load_candidate_cards(
    const std::filesystem::path& root, std::string& error) {
    error.clear();
    std::error_code filesystem_error;
    if (!std::filesystem::is_directory(root, filesystem_error)) {
        error = "Candidate review directory is unavailable: " + root.string();
        return {};
    }

    std::vector<std::filesystem::path> paths;
    for (std::filesystem::recursive_directory_iterator iterator(root, filesystem_error), end;
         iterator != end && !filesystem_error; iterator.increment(filesystem_error)) {
        if (iterator->is_regular_file() && iterator->path().extension() == ".md") {
            paths.push_back(iterator->path());
        }
    }
    if (filesystem_error) {
        error = "Could not scan candidate review directory: " + filesystem_error.message();
        return {};
    }
    std::sort(paths.begin(), paths.end());

    std::vector<CandidateCard> candidates;
    candidates.reserve(paths.size());
    for (const auto& path : paths) {
        CandidateCard card;
        if (!parse_candidate(path, card, error)) return {};
        candidates.push_back(std::move(card));
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return left.title < right.title;
    });
    return candidates;
}

}  // namespace offgrid

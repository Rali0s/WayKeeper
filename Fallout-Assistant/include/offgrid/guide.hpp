#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

struct GuideCard {
    std::string id;
    std::string title;
    std::string answer;
    std::string source_url;
    std::string reviewed_on;
};

struct CandidateCard {
    std::string id;
    std::string title;
    std::string risk;
    std::string trust;
    std::string answer;
    std::string limits;
    std::string source_note;
    std::string source_document;
    std::string source_pdf;
    std::string source_pages;
    std::string source_published;
    std::string cross_check_url;
    std::string cross_checked;
};

std::optional<GuideCard> find_reviewed_card(std::string_view question);
const std::vector<GuideCard>& reviewed_cards();
std::vector<CandidateCard> load_candidate_cards(
    const std::filesystem::path& root, std::string& error);

}  // namespace offgrid

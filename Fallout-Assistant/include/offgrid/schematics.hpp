#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

struct TextSchematic {
    std::string id;
    std::string title;
    std::string category;
    std::string source_note;
    std::string safety_note;
    std::filesystem::path text_path;
};

bool load_text_schematics(
    const std::filesystem::path& catalog_path,
    std::vector<TextSchematic>& schematics,
    std::string& error);

std::optional<std::string> read_text_schematic(
    const TextSchematic& schematic, std::string& error);

// Text schematics use an eight-cell tab stop and never reflow. Trailing and
// leading spaces are preserved so circuit rails remain aligned.
std::vector<std::string> fixed_schematic_lines(
    std::string_view text, std::size_t tab_width = 8);

std::size_t fixed_schematic_width(const std::vector<std::string>& lines);

// Return exactly `width` display cells, padded with spaces when the source line
// is shorter. This lets the viewer pan without wrapping or collapsing blanks.
std::string fixed_schematic_slice(
    std::string_view line, std::size_t horizontal_offset, std::size_t width);

}  // namespace offgrid

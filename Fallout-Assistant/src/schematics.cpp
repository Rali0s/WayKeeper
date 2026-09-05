#include "offgrid/schematics.hpp"

#include <algorithm>
#include <fstream>
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

bool safe_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    return std::none_of(path.begin(), path.end(), [](const auto& component) {
        return component == "..";
    });
}

}  // namespace

bool load_text_schematics(
    const std::filesystem::path& catalog_path,
    std::vector<TextSchematic>& schematics,
    std::string& error) {
    schematics.clear();
    std::ifstream catalog(catalog_path);
    if (!catalog) {
        error = "Text schematic catalog not found: " + catalog_path.string();
        return false;
    }

    const auto base = catalog_path.parent_path();
    std::string line;
    bool first = true;
    while (std::getline(catalog, line)) {
        if (first) {
            first = false;
            continue;
        }
        if (line.empty()) continue;
        const auto fields = split_tabs(line);
        if (fields.size() != 6) {
            error = "Invalid text schematic catalog entry: " + line;
            schematics.clear();
            return false;
        }
        const std::filesystem::path relative = fields[5];
        if (!safe_relative_path(relative)) {
            error = "Unsafe text schematic path: " + fields[5];
            schematics.clear();
            return false;
        }
        const auto source = base / relative;
        if (!std::filesystem::is_regular_file(source)) {
            error = "Text schematic is missing: " + source.string();
            schematics.clear();
            return false;
        }
        schematics.push_back({fields[0], fields[1], fields[2], fields[3], fields[4], source});
    }
    if (schematics.empty()) {
        error = "The text schematic catalog contains no available diagrams.";
        return false;
    }
    return true;
}

std::optional<std::string> read_text_schematic(
    const TextSchematic& schematic, std::string& error) {
    std::ifstream input(schematic.text_path, std::ios::binary);
    if (!input) {
        error = "Could not open text schematic: " + schematic.text_path.string();
        return std::nullopt;
    }
    std::ostringstream content;
    content << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = "Could not read text schematic: " + schematic.text_path.string();
        return std::nullopt;
    }
    return content.str();
}

std::vector<std::string> fixed_schematic_lines(
    const std::string_view text, const std::size_t requested_tab_width) {
    const auto tab_width = std::max<std::size_t>(1, requested_tab_width);
    std::vector<std::string> lines(1);
    for (const unsigned char byte : text) {
        if (byte == '\r') continue;
        if (byte == '\n') {
            lines.emplace_back();
        } else if (byte == '\t') {
            const auto spaces = tab_width - (lines.back().size() % tab_width);
            lines.back().append(spaces, ' ');
        } else if (byte < 0x20 || byte == 0x7f) {
            // Prevent embedded terminal-control bytes from escaping the viewer.
            lines.back().push_back('?');
        } else {
            lines.back().push_back(static_cast<char>(byte));
        }
    }
    if (lines.size() > 1 && lines.back().empty() && text.ends_with('\n')) lines.pop_back();
    if (lines.empty()) lines.emplace_back();
    return lines;
}

std::size_t fixed_schematic_width(const std::vector<std::string>& lines) {
    std::size_t result = 0;
    for (const auto& line : lines) result = std::max(result, line.size());
    return result;
}

std::string fixed_schematic_slice(
    const std::string_view line, const std::size_t horizontal_offset,
    const std::size_t width) {
    if (width == 0) return {};
    std::string result;
    if (horizontal_offset < line.size()) {
        result = std::string(line.substr(
            horizontal_offset, std::min(width, line.size() - horizontal_offset)));
    }
    result.append(width - result.size(), ' ');
    return result;
}

}  // namespace offgrid

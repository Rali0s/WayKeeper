#pragma once

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace offgrid {

struct MapAnnotations;

struct VisibleMapLandmark {
    char symbol{};
    std::string kind;
    std::string name;
    double latitude{};
    double longitude{};
};

struct TerrainMapInfo {
    std::size_t source_width{};
    std::size_t source_height{};
    double west{};
    double south{};
    double east{};
    double north{};
    double minimum_elevation_m{};
    double maximum_elevation_m{};
    std::string projection;
    std::filesystem::path trail_overlay;
    std::size_t trail_feature_count{};
    std::filesystem::path rail_overlay;
    std::size_t rail_feature_count{};
    std::filesystem::path road_overlay;
    std::size_t road_feature_count{};
    std::filesystem::path hydro_overlay;
    std::size_t river_feature_count{};
    std::size_t water_feature_count{};
    std::filesystem::path town_overlay;
    std::size_t town_feature_count{};
    std::vector<VisibleMapLandmark> visible_landmarks;
};

struct TerrainViewport {
    double center_x{0.5};
    double center_y{0.5};
    double zoom{1.0};
};

bool terrain_support_available();
bool inspect_terrain(
    const std::filesystem::path& path, TerrainMapInfo& info, std::string& error);
bool render_terrain_ansi(
    const std::filesystem::path& path,
    std::size_t columns,
    std::size_t rows,
    bool color,
    std::ostream& output,
    TerrainMapInfo& info,
    std::string& error,
    const TerrainViewport& viewport = {},
    const MapAnnotations* annotations = nullptr);
std::vector<std::filesystem::path> discover_terrain_maps(const std::filesystem::path& root);

}  // namespace offgrid

#include "offgrid/map.hpp"
#include "offgrid/map_annotations.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifdef OFFGRID_HAVE_GDAL
#include <gdal_alg.h>
#include <gdal_priv.h>
#include <ogrsf_frmts.h>
#include <ogr_spatialref.h>
#endif

namespace offgrid {
namespace {

struct Rgb {
    int red{};
    int green{};
    int blue{};
};

std::filesystem::path find_overlay(
    const std::filesystem::path& terrain_path, const std::string_view name_token) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(terrain_path.parent_path(), error)) {
        if (error || !entry.is_regular_file()) continue;
        auto extension = entry.path().extension().string();
        auto filename = entry.path().filename().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        std::transform(filename.begin(), filename.end(), filename.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (extension == ".gpkg" && filename.find(name_token) != std::string::npos) {
            return entry.path();
        }
    }
    return {};
}

Rgb terrain_color(const double elevation, const double minimum, const double maximum, const double shade) {
    const double range = std::max(1.0, maximum - minimum);
    const double normalized = std::clamp((elevation - minimum) / range, 0.0, 1.0);
    Rgb base;
    if (elevation <= 0.5) {
        base = {18, 63, 105};
    } else if (normalized < 0.30) {
        const double t = normalized / 0.30;
        base = {static_cast<int>(34 + 55 * t), static_cast<int>(91 + 48 * t),
                static_cast<int>(54 + 21 * t)};
    } else if (normalized < 0.67) {
        const double t = (normalized - 0.30) / 0.37;
        base = {static_cast<int>(89 + 83 * t), static_cast<int>(139 - 38 * t),
                static_cast<int>(75 - 35 * t)};
    } else {
        const double t = (normalized - 0.67) / 0.33;
        base = {static_cast<int>(172 + 72 * t), static_cast<int>(101 + 132 * t),
                static_cast<int>(40 + 188 * t)};
    }
    const double light = 0.35 + 0.90 * std::clamp(shade, 0.0, 1.0);
    return {
        std::clamp(static_cast<int>(base.red * light), 0, 255),
        std::clamp(static_cast<int>(base.green * light), 0, 255),
        std::clamp(static_cast<int>(base.blue * light), 0, 255)};
}

double hillshade(
    const std::vector<float>& values, const std::size_t width, const std::size_t height,
    const std::size_t x, const std::size_t y, const double dx_m, const double dy_m) {
    const auto at = [&](const std::size_t px, const std::size_t py) {
        return static_cast<double>(values[std::min(py, height - 1) * width + std::min(px, width - 1)]);
    };
    const std::size_t left = x == 0 ? x : x - 1;
    const std::size_t right = std::min(x + 1, width - 1);
    const std::size_t top = y == 0 ? y : y - 1;
    const std::size_t bottom = std::min(y + 1, height - 1);
    const double dzdx = (at(right, y) - at(left, y)) / std::max(1.0, 2.0 * dx_m);
    const double dzdy = (at(x, top) - at(x, bottom)) / std::max(1.0, 2.0 * dy_m);

    constexpr double pi = 3.14159265358979323846;
    const double azimuth = 315.0 * pi / 180.0;
    const double altitude = 45.0 * pi / 180.0;
    const double sun_x = std::sin(azimuth) * std::cos(altitude);
    const double sun_y = std::cos(azimuth) * std::cos(altitude);
    const double sun_z = std::sin(altitude);
    double normal_x = -dzdx;
    double normal_y = -dzdy;
    double normal_z = 1.0;
    const double length = std::sqrt(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
    normal_x /= length;
    normal_y /= length;
    normal_z /= length;
    return std::clamp(normal_x * sun_x + normal_y * sun_y + normal_z * sun_z, 0.0, 1.0);
}

#ifdef OFFGRID_HAVE_GDAL
std::size_t overlay_feature_count(
    const std::filesystem::path& overlay, const std::string_view layer_name) {
    if (overlay.empty()) return 0;
    auto* dataset = static_cast<GDALDataset*>(GDALOpenEx(
        overlay.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
        nullptr, nullptr, nullptr));
    if (!dataset) return 0;
    auto* layer = dataset->GetLayerByName(std::string(layer_name).c_str());
    const auto count = layer && layer->GetFeatureCount() > 0
        ? static_cast<std::size_t>(layer->GetFeatureCount()) : 0;
    GDALClose(dataset);
    return count;
}

bool rasterize_overlay(
    const TerrainMapInfo& info, const std::filesystem::path& overlay,
    const std::string_view layer_name, const std::size_t width, const std::size_t height,
    std::vector<std::uint8_t>& mask, const std::size_t dilation) {
    mask.assign(width * height, 0);
    if (overlay.empty()) return true;

    auto* trails = static_cast<GDALDataset*>(GDALOpenEx(
        overlay.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
        nullptr, nullptr, nullptr));
    if (!trails) return false;
    auto* layer = trails->GetLayerByName(std::string(layer_name).c_str());
    if (!layer) layer = trails->GetLayer(0);
    if (!layer) {
        GDALClose(trails);
        return false;
    }
    layer->SetSpatialFilterRect(info.west, info.south, info.east, info.north);

    auto* driver = GetGDALDriverManager()->GetDriverByName("MEM");
    auto* raster = driver ? driver->Create(
        "", static_cast<int>(width), static_cast<int>(height), 1, GDT_Byte, nullptr) : nullptr;
    if (!raster) {
        GDALClose(trails);
        return false;
    }
    double transform[6]{
        info.west, (info.east - info.west) / static_cast<double>(width), 0.0,
        info.north, 0.0, (info.south - info.north) / static_cast<double>(height)};
    raster->SetGeoTransform(transform);
    OGRSpatialReference wgs84;
    wgs84.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    wgs84.SetWellKnownGeogCS("WGS84");
    raster->SetSpatialRef(&wgs84);
    raster->GetRasterBand(1)->Fill(0.0);

    int band = 1;
    OGRLayerH layer_handle = reinterpret_cast<OGRLayerH>(layer);
    double burn = 255.0;
    char** options = nullptr;
    options = CSLSetNameValue(options, "ALL_TOUCHED", "TRUE");
    const auto rasterize_status = GDALRasterizeLayers(
        raster, 1, &band, 1, &layer_handle, nullptr, nullptr, &burn, options, nullptr, nullptr);
    CSLDestroy(options);
    const auto read_status = raster->GetRasterBand(1)->RasterIO(
        GF_Read, 0, 0, static_cast<int>(width), static_cast<int>(height), mask.data(),
        static_cast<int>(width), static_cast<int>(height), GDT_Byte, 0, 0, nullptr);
    if (rasterize_status == CE_None && read_status == CE_None && dilation > 0) {
        const auto original = mask;
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                if (!original[y * width + x]) continue;
                const auto y0 = y > dilation ? y - dilation : 0;
                const auto x0 = x > dilation ? x - dilation : 0;
                for (auto ny = y0; ny <= std::min(height - 1, y + dilation); ++ny) {
                    for (auto nx = x0; nx <= std::min(width - 1, x + dilation); ++nx) {
                        mask[ny * width + nx] = 255;
                    }
                }
            }
        }
    }
    GDALClose(raster);
    GDALClose(trails);
    return rasterize_status == CE_None && read_status == CE_None;
}

struct LandmarkCandidate {
    std::string name;
    double latitude{};
    double longitude{};
    double importance{};
};

std::string compact_landmark_name(std::string name) {
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    name.erase(0, first);
    const auto last = name.find_last_not_of(" \t\r\n");
    name.erase(last + 1);
    for (const std::string_view prefix : {"City of ", "Village of ", "Town of "}) {
        if (name.rfind(prefix, 0) == 0) {
            name.erase(0, prefix.size());
            break;
        }
    }
    return name;
}

void place_landmarks(
    const TerrainMapInfo& info, const std::filesystem::path& overlay,
    const std::string_view layer_name, const std::string_view name_field,
    const std::string_view importance_field, const std::string_view kind,
    const char symbol, const std::size_t limit,
    const std::size_t width, const std::size_t height,
    std::vector<char>& landmark_mask,
    std::vector<VisibleMapLandmark>& visible) {
    if (overlay.empty() || limit == 0) return;
    auto* dataset = static_cast<GDALDataset*>(GDALOpenEx(
        overlay.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
        nullptr, nullptr, nullptr));
    if (!dataset) return;
    auto* layer = dataset->GetLayerByName(std::string(layer_name).c_str());
    if (!layer) {
        GDALClose(dataset);
        return;
    }
    layer->SetSpatialFilterRect(info.west, info.south, info.east, info.north);
    std::vector<LandmarkCandidate> candidates;
    std::set<std::string> seen;
    layer->ResetReading();
    while (auto* feature = layer->GetNextFeature()) {
        auto* geometry = feature->GetGeometryRef();
        const auto name_index = feature->GetFieldIndex(std::string(name_field).c_str());
        const char* raw_name = name_index >= 0 && feature->IsFieldSetAndNotNull(name_index)
            ? feature->GetFieldAsString(name_index) : nullptr;
        std::string name = raw_name ? compact_landmark_name(raw_name) : std::string{};
        if (!geometry || name.empty()) {
            OGRFeature::DestroyFeature(feature);
            continue;
        }
        OGRPoint point;
        OGRErr point_status = OGRERR_NONE;
        if (wkbFlatten(geometry->getGeometryType()) == wkbPoint) {
            const auto* source = geometry->toPoint();
            point.setX(source->getX());
            point.setY(source->getY());
        } else {
            point_status = geometry->Centroid(&point);
        }
        if (point_status != OGRERR_NONE ||
            point.getX() < info.west || point.getX() > info.east ||
            point.getY() < info.south || point.getY() > info.north) {
            OGRFeature::DestroyFeature(feature);
            continue;
        }
        if (!seen.insert(name).second) {
            OGRFeature::DestroyFeature(feature);
            continue;
        }
        double importance = 1.0;
        if (!importance_field.empty()) {
            const auto importance_index = feature->GetFieldIndex(
                std::string(importance_field).c_str());
            if (importance_index >= 0 && feature->IsFieldSetAndNotNull(importance_index)) {
                importance = feature->GetFieldAsDouble(importance_index);
            }
        } else if (raw_name && std::string_view(raw_name).rfind("City of ", 0) == 0) {
            importance = 3.0;
        } else if (raw_name && std::string_view(raw_name).rfind("Village of ", 0) == 0) {
            importance = 2.0;
        }
        candidates.push_back({std::move(name), point.getY(), point.getX(), importance});
        OGRFeature::DestroyFeature(feature);
    }
    GDALClose(dataset);
    std::sort(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        if (left.importance != right.importance) return left.importance > right.importance;
        return left.name < right.name;
    });

    std::size_t placed = 0;
    for (const auto& candidate : candidates) {
        if (placed >= limit) break;
        const auto x = static_cast<std::size_t>(std::clamp<long long>(
            std::llround((candidate.longitude - info.west) /
                std::max(1e-12, info.east - info.west) * static_cast<double>(width - 1)),
            0, static_cast<long long>(width - 1)));
        const auto y = static_cast<std::size_t>(std::clamp<long long>(
            std::llround((info.north - candidate.latitude) /
                std::max(1e-12, info.north - info.south) * static_cast<double>(height - 1)),
            0, static_cast<long long>(height - 1)));
        bool occupied = false;
        const auto x0 = x > 2 ? x - 2 : 0;
        const auto y0 = y > 1 ? y - 1 : 0;
        for (auto near_y = y0; near_y <= std::min(height - 1, y + 1) && !occupied; ++near_y) {
            for (auto near_x = x0; near_x <= std::min(width - 1, x + 2); ++near_x) {
                if (landmark_mask[near_y * width + near_x]) {
                    occupied = true;
                    break;
                }
            }
        }
        if (occupied) continue;
        landmark_mask[y * width + x] = symbol;
        visible.push_back({symbol, std::string(kind), candidate.name,
                           candidate.latitude, candidate.longitude});
        ++placed;
    }
}
#endif

}  // namespace

bool terrain_support_available() {
#ifdef OFFGRID_HAVE_GDAL
    return true;
#else
    return false;
#endif
}

bool inspect_terrain(
    const std::filesystem::path& path, TerrainMapInfo& info, std::string& error) {
#ifndef OFFGRID_HAVE_GDAL
    (void)path;
    (void)info;
    error = "Terrain support was built without GDAL.";
    return false;
#else
    GDALAllRegister();
    auto* dataset = static_cast<GDALDataset*>(GDALOpen(path.string().c_str(), GA_ReadOnly));
    if (!dataset) {
        error = "GDAL could not open terrain file: " + path.string();
        return false;
    }
    info.source_width = static_cast<std::size_t>(dataset->GetRasterXSize());
    info.source_height = static_cast<std::size_t>(dataset->GetRasterYSize());
    double transform[6]{};
    if (dataset->GetGeoTransform(transform) == CE_None) {
        info.west = transform[0];
        info.north = transform[3];
        info.east = transform[0] + transform[1] * dataset->GetRasterXSize();
        info.south = transform[3] + transform[5] * dataset->GetRasterYSize();
    }
    if (const auto* reference = dataset->GetSpatialRef()) {
        const char* name = reference->GetName();
        if (name) info.projection = name;
    }
    auto* band = dataset->GetRasterBand(1);
    if (!band) {
        GDALClose(dataset);
        error = "Terrain file has no raster band.";
        return false;
    }
    double minimum = 0.0;
    double maximum = 0.0;
    double mean = 0.0;
    double deviation = 0.0;
    if (band->GetStatistics(false, true, &minimum, &maximum, &mean, &deviation) != CE_None) {
        GDALClose(dataset);
        error = "Could not calculate terrain elevation statistics.";
        return false;
    }
    info.minimum_elevation_m = minimum;
    info.maximum_elevation_m = maximum;
    GDALClose(dataset);
    info.trail_overlay = find_overlay(path, "trail");
    if (!info.trail_overlay.empty()) {
        auto* trails = static_cast<GDALDataset*>(GDALOpenEx(
            info.trail_overlay.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
            nullptr, nullptr, nullptr));
        if (trails) {
            auto* layer = trails->GetLayerByName("hiking_trails");
            if (!layer) layer = trails->GetLayer(0);
            if (layer) info.trail_feature_count = static_cast<std::size_t>(layer->GetFeatureCount());
            GDALClose(trails);
        }
    }
    info.rail_overlay = find_overlay(path, "railroad");
    if (!info.rail_overlay.empty()) {
        auto* rails = static_cast<GDALDataset*>(GDALOpenEx(
            info.rail_overlay.string().c_str(), GDAL_OF_VECTOR | GDAL_OF_READONLY,
            nullptr, nullptr, nullptr));
        if (rails) {
            auto* layer = rails->GetLayerByName("railroads");
            if (!layer) layer = rails->GetLayer(0);
            if (layer) info.rail_feature_count = static_cast<std::size_t>(layer->GetFeatureCount());
            GDALClose(rails);
        }
    }
    info.road_overlay = find_overlay(path, "primary-roads");
    info.road_feature_count = overlay_feature_count(info.road_overlay, "major_roads");
    info.hydro_overlay = find_overlay(path, "hydrography");
    info.river_feature_count = overlay_feature_count(info.hydro_overlay, "rivers");
    info.water_feature_count =
        overlay_feature_count(info.hydro_overlay, "waterbodies") +
        overlay_feature_count(info.hydro_overlay, "water_areas");
    info.town_overlay = find_overlay(path, "towns");
    info.town_feature_count = overlay_feature_count(info.town_overlay, "towns");
    info.visible_landmarks.clear();
    return true;
#endif
}

bool render_terrain_ansi(
    const std::filesystem::path& path,
    const std::size_t columns,
    const std::size_t rows,
    const bool color,
    std::ostream& output,
    TerrainMapInfo& info,
    std::string& error,
    const TerrainViewport& viewport,
    const MapAnnotations* annotations) {
#ifndef OFFGRID_HAVE_GDAL
    (void)path; (void)columns; (void)rows; (void)color; (void)output; (void)info; (void)viewport;
    (void)annotations;
    error = "Terrain support was built without GDAL.";
    return false;
#else
    if (columns < 8 || rows < 4) {
        error = "Terrain viewport is too small.";
        return false;
    }
    if (!inspect_terrain(path, info, error)) return false;
    auto* dataset = static_cast<GDALDataset*>(GDALOpen(path.string().c_str(), GA_ReadOnly));
    if (!dataset) {
        error = "GDAL could not reopen terrain file.";
        return false;
    }
    auto* band = dataset->GetRasterBand(1);
    const auto source_width = dataset->GetRasterXSize();
    const auto source_height = dataset->GetRasterYSize();
    const auto zoom = std::clamp(
        viewport.zoom, 1.0, static_cast<double>(std::max(source_width, source_height)));
    const auto window_width = std::max(1, static_cast<int>(std::lround(source_width / zoom)));
    const auto window_height = std::max(1, static_cast<int>(std::lround(source_height / zoom)));
    const auto center_x = std::clamp(viewport.center_x, 0.0, 1.0) * source_width;
    const auto center_y = std::clamp(viewport.center_y, 0.0, 1.0) * source_height;
    const auto x_offset = std::clamp(
        static_cast<int>(std::lround(center_x - window_width / 2.0)),
        0, source_width - window_width);
    const auto y_offset = std::clamp(
        static_cast<int>(std::lround(center_y - window_height / 2.0)),
        0, source_height - window_height);

    double transform[6]{};
    if (dataset->GetGeoTransform(transform) == CE_None) {
        const auto coordinate = [&](const double x, const double y) {
            return std::pair{
                transform[0] + transform[1] * x + transform[2] * y,
                transform[3] + transform[4] * x + transform[5] * y};
        };
        const auto northwest = coordinate(x_offset, y_offset);
        const auto northeast = coordinate(x_offset + window_width, y_offset);
        const auto southwest = coordinate(x_offset, y_offset + window_height);
        const auto southeast = coordinate(x_offset + window_width, y_offset + window_height);
        info.west = std::min({northwest.first, northeast.first, southwest.first, southeast.first});
        info.east = std::max({northwest.first, northeast.first, southwest.first, southeast.first});
        info.south = std::min({northwest.second, northeast.second, southwest.second, southeast.second});
        info.north = std::max({northwest.second, northeast.second, southwest.second, southeast.second});
    }

    const std::size_t pixel_rows = rows * 2;
    std::vector<float> values(columns * pixel_rows);
    GDALRasterIOExtraArg arguments;
    INIT_RASTERIO_EXTRA_ARG(arguments);
    arguments.eResampleAlg = GRIORA_Bilinear;
    const auto status = band->RasterIO(
        GF_Read, x_offset, y_offset, window_width, window_height,
        values.data(), static_cast<int>(columns), static_cast<int>(pixel_rows),
        GDT_Float32, 0, 0, &arguments);
    GDALClose(dataset);
    if (status != CE_None) {
        error = "GDAL failed while resampling terrain data.";
        return false;
    }

    std::vector<std::uint8_t> trail_mask;
    if (!rasterize_overlay(
            info, info.trail_overlay, "hiking_trails", columns, pixel_rows,
            trail_mask, 0)) {
        trail_mask.assign(columns * pixel_rows, 0);
    }
    std::vector<std::uint8_t> rail_mask;
    if (!rasterize_overlay(
            info, info.rail_overlay, "railroads", columns, pixel_rows, rail_mask, 0)) {
        rail_mask.assign(columns * pixel_rows, 0);
    }
    std::vector<std::uint8_t> road_mask;
    if (!rasterize_overlay(
            info, info.road_overlay, "major_roads", columns, pixel_rows, road_mask, 0)) {
        road_mask.assign(columns * pixel_rows, 0);
    }
    std::vector<std::uint8_t> water_mask(columns * pixel_rows, 0);
    for (const std::string_view layer : {"rivers", "waterbodies", "water_areas"}) {
        std::vector<std::uint8_t> layer_mask;
        if (!rasterize_overlay(
                info, info.hydro_overlay, layer, columns, pixel_rows, layer_mask, 0)) {
            continue;
        }
        for (std::size_t pixel = 0; pixel < water_mask.size(); ++pixel) {
            water_mask[pixel] = std::max(water_mask[pixel], layer_mask[pixel]);
        }
    }

    std::vector<char> landmark_mask(columns * pixel_rows, '\0');
    const auto scale = std::max<std::size_t>(1, columns / 80);
    place_landmarks(
        info, info.town_overlay, "towns", "gaz_name", {},
        "TOWN", '@', 12 * scale, columns, pixel_rows,
        landmark_mask, info.visible_landmarks);
    place_landmarks(
        info, info.hydro_overlay, "waterbodies", "GNIS_NAME", "AREASQKM",
        "LAKE", 'O', 6 * scale, columns, pixel_rows,
        landmark_mask, info.visible_landmarks);
    place_landmarks(
        info, info.hydro_overlay, "rivers", "GNIS_NAME", "StreamOrde",
        "RIVER", '~', 6 * scale, columns, pixel_rows,
        landmark_mask, info.visible_landmarks);
    place_landmarks(
        info, info.road_overlay, "road_markers", "PRIME_NAME", {},
        "ROAD", '=', 4 * scale, columns, pixel_rows,
        landmark_mask, info.visible_landmarks);

    std::vector<std::uint8_t> waypoint_mask(columns * pixel_rows, 0);
    std::vector<char> marker_mask(columns * pixel_rows, '\0');
    const auto project = [&](const MapMarker& marker) -> std::optional<std::pair<int, int>> {
        if (marker.longitude < info.west || marker.longitude > info.east ||
            marker.latitude < info.south || marker.latitude > info.north) return std::nullopt;
        const auto x = static_cast<int>(std::lround(
            (marker.longitude - info.west) / std::max(1e-12, info.east - info.west) *
            static_cast<double>(columns - 1)));
        const auto y = static_cast<int>(std::lround(
            (info.north - marker.latitude) / std::max(1e-12, info.north - info.south) *
            static_cast<double>(pixel_rows - 1)));
        return std::pair{x, y};
    };
    const auto draw_line = [&](std::pair<int, int> start, std::pair<int, int> end) {
        auto [x0, y0] = start;
        const auto [x1, y1] = end;
        const auto dx = std::abs(x1 - x0);
        const auto sx = x0 < x1 ? 1 : -1;
        const auto dy = -std::abs(y1 - y0);
        const auto sy = y0 < y1 ? 1 : -1;
        auto line_error = dx + dy;
        for (;;) {
            if (x0 >= 0 && y0 >= 0 && x0 < static_cast<int>(columns) &&
                y0 < static_cast<int>(pixel_rows)) {
                waypoint_mask[static_cast<std::size_t>(y0) * columns +
                              static_cast<std::size_t>(x0)] = 255;
            }
            if (x0 == x1 && y0 == y1) break;
            const auto twice = 2 * line_error;
            if (twice >= dy) { line_error += dy; x0 += sx; }
            if (twice <= dx) { line_error += dx; y0 += sy; }
        }
    };
    if (annotations) {
        if (annotations->connect_waypoints && annotations->markers.size() >= 2) {
            for (std::size_t marker = 1; marker < annotations->markers.size(); ++marker) {
                const auto start = project(annotations->markers[marker - 1]);
                const auto end = project(annotations->markers[marker]);
                if (start && end) draw_line(*start, *end);
            }
        }
        for (const auto& marker : annotations->markers) {
            if (const auto position = project(marker)) {
                marker_mask[static_cast<std::size_t>(position->second) * columns +
                            static_cast<std::size_t>(position->first)] = map_marker_symbol(marker.type);
            }
        }
    }

    const double center_latitude = (info.north + info.south) / 2.0;
    constexpr double pi = 3.14159265358979323846;
    const double dx_m = std::abs(info.east - info.west) / static_cast<double>(columns) *
        111320.0 * std::cos(center_latitude * pi / 180.0);
    const double dy_m = std::abs(info.north - info.south) / static_cast<double>(pixel_rows) * 110574.0;
    constexpr std::string_view ramp = " .:-=+*#%@";

    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            const std::size_t top_y = row * 2;
            const std::size_t bottom_y = top_y + 1;
            const double top_elevation = values[top_y * columns + column];
            const double bottom_elevation = values[bottom_y * columns + column];
            const double top_shade = hillshade(values, columns, pixel_rows, column, top_y, dx_m, dy_m);
            const double bottom_shade = hillshade(values, columns, pixel_rows, column, bottom_y, dx_m, dy_m);
            const bool top_trail = trail_mask[top_y * columns + column] != 0;
            const bool bottom_trail = trail_mask[bottom_y * columns + column] != 0;
            const bool top_rail = rail_mask[top_y * columns + column] != 0;
            const bool bottom_rail = rail_mask[bottom_y * columns + column] != 0;
            const bool top_road = road_mask[top_y * columns + column] != 0;
            const bool bottom_road = road_mask[bottom_y * columns + column] != 0;
            const bool top_water = water_mask[top_y * columns + column] != 0;
            const bool bottom_water = water_mask[bottom_y * columns + column] != 0;
            const bool top_waypoint = waypoint_mask[top_y * columns + column] != 0;
            const bool bottom_waypoint = waypoint_mask[bottom_y * columns + column] != 0;
            const char marker = marker_mask[top_y * columns + column]
                ? marker_mask[top_y * columns + column]
                : marker_mask[bottom_y * columns + column];
            const char landmark = landmark_mask[top_y * columns + column]
                ? landmark_mask[top_y * columns + column]
                : landmark_mask[bottom_y * columns + column];
            if (marker) {
                if (color) output << "\033[1;38;2;80;255;90m\033[48;2;8;20;8m";
                output << marker;
                continue;
            }
            if (landmark) {
                if (color) {
                    if (landmark == '@') {
                        output << "\033[1;38;2;255;255;255m\033[48;2;190;45;55m";
                    } else if (landmark == '=') {
                        output << "\033[1;38;2;245;245;225m\033[48;2;45;45;40m";
                    } else {
                        output << "\033[1;38;2;120;220;255m\033[48;2;5;35;85m";
                    }
                }
                output << landmark;
                continue;
            }
            if (color) {
                const auto top = top_waypoint ? Rgb{255, 140, 20}
                    : top_rail ? Rgb{70, 230, 255}
                    : top_road ? Rgb{235, 235, 220}
                    : top_water ? Rgb{35, 120, 230}
                    : top_trail ? Rgb{255, 215, 40}
                    : terrain_color(top_elevation, info.minimum_elevation_m,
                                    info.maximum_elevation_m, top_shade);
                const auto bottom = bottom_waypoint ? Rgb{255, 140, 20}
                    : bottom_rail ? Rgb{70, 230, 255}
                    : bottom_road ? Rgb{235, 235, 220}
                    : bottom_water ? Rgb{35, 120, 230}
                    : bottom_trail ? Rgb{255, 215, 40}
                    : terrain_color(bottom_elevation, info.minimum_elevation_m,
                                    info.maximum_elevation_m, bottom_shade);
                output << "\033[38;2;" << top.red << ';' << top.green << ';' << top.blue
                       << "m\033[48;2;" << bottom.red << ';' << bottom.green << ';' << bottom.blue
                       << "m▀";
            } else {
                const double average_elevation = (top_elevation + bottom_elevation) / 2.0;
                const double elevation_level = std::clamp(
                    (average_elevation - info.minimum_elevation_m) /
                        std::max(1.0, info.maximum_elevation_m - info.minimum_elevation_m),
                    0.0, 1.0);
                const double combined = 0.72 * elevation_level +
                    0.28 * ((top_shade + bottom_shade) / 2.0);
                const std::size_t index = std::min(
                    ramp.size() - 1, static_cast<std::size_t>(combined * (ramp.size() - 1)));
                const bool rail = top_rail || bottom_rail;
                const bool road = top_road || bottom_road;
                const bool water = top_water || bottom_water;
                const bool trail = top_trail || bottom_trail;
                const bool waypoint = top_waypoint || bottom_waypoint;
                output << (waypoint ? '+' : rail && road ? 'X' : rail ? '#'
                    : road ? '=' : water ? '~' : trail ? '*' : ramp[index]);
            }
        }
        if (color) output << "\033[0m";
        output << '\n';
    }
    return true;
#endif
}

std::vector<std::filesystem::path> discover_terrain_maps(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> maps;
    std::error_code error;
    if (!std::filesystem::exists(root, error)) return maps;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, error)) {
        if (error || !entry.is_regular_file()) continue;
        auto extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (extension == ".tif" || extension == ".tiff") maps.push_back(entry.path());
    }
    std::sort(maps.begin(), maps.end());
    return maps;
}

}  // namespace offgrid

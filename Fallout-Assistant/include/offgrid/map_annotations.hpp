#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

struct MapMarker {
    std::string id;
    double latitude{};
    double longitude{};
    std::string type;
    std::string name;
};

struct MapAnnotations {
    bool connect_waypoints{true};
    std::vector<MapMarker> markers;
};

class MapAnnotationStore {
public:
    explicit MapAnnotationStore(std::filesystem::path path);
    const std::filesystem::path& path() const;
    bool load(MapAnnotations& annotations, std::string& error) const;
    bool save(const MapAnnotations& annotations, std::string& error) const;

private:
    std::filesystem::path path_;
};

const std::vector<std::string>& map_marker_types();
bool valid_map_marker_type(std::string_view type);
char map_marker_symbol(std::string_view type);
double waypoint_distance_miles(const std::vector<MapMarker>& markers);

}  // namespace offgrid

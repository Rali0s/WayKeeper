#include "offgrid/map_annotations.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace offgrid {
namespace {

constexpr std::string_view magic = "WAYKEEPER-MAP-ANNOTATIONS-1";

std::string clean_field(std::string value) {
    std::replace(value.begin(), value.end(), '\t', ' ');
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    value.erase(std::remove(value.begin(), value.end(), '\n'), value.end());
    if (value.size() > 80) value.resize(80);
    return value;
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream input(line);
    std::string field;
    while (std::getline(input, field, '\t')) fields.push_back(field);
    return fields;
}

}  // namespace

MapAnnotationStore::MapAnnotationStore(std::filesystem::path path) : path_(std::move(path)) {}

const std::filesystem::path& MapAnnotationStore::path() const { return path_; }

bool MapAnnotationStore::load(MapAnnotations& annotations, std::string& error) const {
    annotations = {};
    if (!std::filesystem::exists(path_)) return true;
    std::ifstream input(path_);
    std::string line;
    if (!std::getline(input, line) || line != magic) {
        error = "Unsupported map annotation format: " + path_.string();
        return false;
    }
    while (std::getline(input, line)) {
        const auto fields = split_tabs(line);
        if (fields.size() >= 3 && fields[0] == "META" && fields[1] == "connect_waypoints") {
            annotations.connect_waypoints = fields[2] != "0";
        } else if (fields.size() >= 6 && fields[0] == "POINT") {
            MapMarker marker;
            marker.id = fields[1];
            try {
                marker.latitude = std::stod(fields[2]);
                marker.longitude = std::stod(fields[3]);
            } catch (...) {
                continue;
            }
            marker.type = fields[4];
            marker.name = fields[5];
            if (marker.latitude >= -90.0 && marker.latitude <= 90.0 &&
                marker.longitude >= -180.0 && marker.longitude <= 180.0 &&
                valid_map_marker_type(marker.type) && !marker.name.empty()) {
                annotations.markers.push_back(std::move(marker));
            }
        }
    }
    return true;
}

bool MapAnnotationStore::save(const MapAnnotations& annotations, std::string& error) const {
    std::error_code filesystem_error;
    std::filesystem::create_directories(path_.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create map overlay directory: " + filesystem_error.message();
        return false;
    }
    auto temporary = path_;
    temporary += ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        error = "Could not write map overlay: " + temporary.string();
        return false;
    }
    output << magic << "\nMETA\tconnect_waypoints\t"
           << (annotations.connect_waypoints ? 1 : 0) << '\n';
    output << std::setprecision(10);
    for (const auto& marker : annotations.markers) {
        if (!valid_map_marker_type(marker.type) || marker.name.empty()) continue;
        output << "POINT\t" << clean_field(marker.id) << '\t' << marker.latitude << '\t'
               << marker.longitude << '\t' << clean_field(marker.type) << '\t'
               << clean_field(marker.name) << '\n';
    }
    output.close();
    if (!output) {
        std::filesystem::remove(temporary);
        error = "Could not finish map overlay write.";
        return false;
    }
    std::filesystem::rename(temporary, path_, filesystem_error);
    if (filesystem_error) {
        std::filesystem::remove(path_, filesystem_error);
        filesystem_error.clear();
        std::filesystem::rename(temporary, path_, filesystem_error);
    }
    if (filesystem_error) {
        std::filesystem::remove(temporary);
        error = "Could not install map overlay: " + filesystem_error.message();
        return false;
    }
    return true;
}

const std::vector<std::string>& map_marker_types() {
    static const std::vector<std::string> types{
        "Homestead", "Compound", "Town", "Village", "Market", "Water", "Food",
        "Abandoned Structure", "Scavenge"};
    return types;
}

bool valid_map_marker_type(const std::string_view type) {
    const auto& types = map_marker_types();
    return std::find(types.begin(), types.end(), type) != types.end();
}

char map_marker_symbol(const std::string_view type) {
    if (type == "Homestead") return 'H';
    if (type == "Compound") return 'C';
    if (type == "Town") return 'T';
    if (type == "Village") return 'V';
    if (type == "Market") return 'M';
    if (type == "Water") return 'W';
    if (type == "Food") return 'F';
    if (type == "Abandoned Structure") return 'A';
    return 'S';
}

double waypoint_distance_miles(const std::vector<MapMarker>& markers) {
    if (markers.size() < 2) return 0.0;
    constexpr double pi = 3.14159265358979323846;
    constexpr double earth_radius_miles = 3958.7613;
    const auto radians = [&](const double degrees) { return degrees * pi / 180.0; };
    double total = 0.0;
    for (std::size_t index = 1; index < markers.size(); ++index) {
        const auto lat1 = radians(markers[index - 1].latitude);
        const auto lat2 = radians(markers[index].latitude);
        const auto delta_lat = lat2 - lat1;
        const auto delta_lon = radians(markers[index].longitude - markers[index - 1].longitude);
        const auto a = std::sin(delta_lat / 2.0) * std::sin(delta_lat / 2.0) +
            std::cos(lat1) * std::cos(lat2) *
            std::sin(delta_lon / 2.0) * std::sin(delta_lon / 2.0);
        total += earth_radius_miles * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    }
    return total;
}

}  // namespace offgrid

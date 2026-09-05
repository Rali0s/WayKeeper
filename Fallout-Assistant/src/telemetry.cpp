#include "offgrid/telemetry.hpp"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <vector>

namespace offgrid {
namespace {

std::vector<std::string_view> split(const std::string_view line, const char delimiter) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t end = line.find(delimiter, start);
        fields.push_back(line.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    return fields;
}

std::optional<double> parse_double(const std::string_view text) {
    std::string owned(text);
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(owned.c_str(), &end);
    if (errno != 0 || end != owned.c_str() + owned.size()) return std::nullopt;
    return value;
}

std::optional<MeasurementQuality> parse_quality(const std::string_view text) {
    if (text == "measured") return MeasurementQuality::Measured;
    if (text == "estimated") return MeasurementQuality::Estimated;
    if (text == "simulated") return MeasurementQuality::Simulated;
    if (text == "fault") return MeasurementQuality::Fault;
    return std::nullopt;
}

}  // namespace

std::optional<TelemetryReading> parse_telemetry(const std::string_view line) {
    const auto fields = split(line, '|');
    if (fields.size() != 7 || fields[0] != "OGP1") return std::nullopt;
    if (fields[1].empty() || fields[2].empty() || fields[4].empty()) return std::nullopt;

    const auto value = parse_double(fields[3]);
    std::int64_t timestamp{};
    const auto timestamp_result = std::from_chars(
        fields[5].data(), fields[5].data() + fields[5].size(), timestamp);
    const auto quality = parse_quality(fields[6]);
    if (!value || timestamp_result.ec != std::errc{} ||
        timestamp_result.ptr != fields[5].data() + fields[5].size() || !quality) {
        return std::nullopt;
    }

    return TelemetryReading{
        std::string(fields[1]), std::string(fields[2]), *value, std::string(fields[4]),
        timestamp, *quality};
}

const char* quality_name(const MeasurementQuality quality) {
    switch (quality) {
        case MeasurementQuality::Measured: return "measured";
        case MeasurementQuality::Estimated: return "estimated";
        case MeasurementQuality::Simulated: return "simulated";
        case MeasurementQuality::Fault: return "fault";
    }
    return "unknown";
}

}  // namespace offgrid


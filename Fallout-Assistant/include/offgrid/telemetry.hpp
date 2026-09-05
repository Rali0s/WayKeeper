#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace offgrid {

enum class MeasurementQuality {
    Measured,
    Estimated,
    Simulated,
    Fault
};

struct TelemetryReading {
    std::string source;
    std::string metric;
    double value{};
    std::string unit;
    std::int64_t unix_milliseconds{};
    MeasurementQuality quality{MeasurementQuality::Fault};
};

// OGP1|source|metric|value|unit|unix_ms|quality
std::optional<TelemetryReading> parse_telemetry(std::string_view line);
const char* quality_name(MeasurementQuality quality);

}  // namespace offgrid


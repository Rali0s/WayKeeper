#include "offgrid/energy.hpp"

#include <algorithm>
#include <limits>

namespace offgrid {

EnergyEstimate estimate_query_cost(
    const InferenceProfile& profile,
    const std::size_t input_tokens,
    const std::size_t output_tokens,
    const double net_solar_watts) {
    const std::size_t total_tokens = input_tokens + output_tokens;
    const double generation_seconds = profile.tokens_per_second > 0.0
        ? static_cast<double>(total_tokens) / profile.tokens_per_second
        : 0.0;
    const double runtime_seconds = std::max(0.0, profile.fixed_seconds + generation_seconds);
    const double watt_hours = std::max(0.0, profile.system_watts) * runtime_seconds / 3600.0;
    const double mwh_per_token = total_tokens > 0
        ? watt_hours * 1000.0 / static_cast<double>(total_tokens)
        : 0.0;
    const double solar_minutes = net_solar_watts > 0.0
        ? watt_hours / net_solar_watts * 60.0
        : std::numeric_limits<double>::infinity();

    return {runtime_seconds, watt_hours, mwh_per_token, solar_minutes};
}

RouteDecision choose_route(
    const bool reviewed_card_hit,
    const bool manual_search_hit,
    const PowerSnapshot& power,
    const InferenceProfile& small_model,
    const InferenceProfile& large_model,
    const std::size_t input_tokens,
    const std::size_t output_tokens) {
    if (reviewed_card_hit) {
        return {ResponseRoute::StaticCard, {},
                "A reviewed safety card answers this without running a model."};
    }

    if (manual_search_hit) {
        return {ResponseRoute::ManualSearch, {},
                "An indexed manual passage answers this with lower energy and better provenance."};
    }

    const auto small = estimate_query_cost(
        small_model, input_tokens, output_tokens, power.net_solar_watts);
    const double energy_above_reserve = power.usable_watt_hours - power.reserve_watt_hours;
    if (energy_above_reserve < small.watt_hours) {
        return {ResponseRoute::Defer, small,
                "The projected query would cross the configured battery reserve."};
    }

    const auto large = estimate_query_cost(
        large_model, input_tokens, output_tokens, power.net_solar_watts);
    const bool solar_surplus = power.net_solar_watts >= large_model.system_watts;
    const bool ample_stored_energy = energy_above_reserve >= large.watt_hours * 10.0;
    if (solar_surplus && ample_stored_energy) {
        return {ResponseRoute::LargeModel, large,
                "Solar input and stored energy are sufficient for the higher-capability model."};
    }

    return {ResponseRoute::SmallModel, small,
            "The compact model stays above reserve and costs less energy."};
}

const char* route_name(const ResponseRoute route) {
    switch (route) {
        case ResponseRoute::StaticCard: return "static-card";
        case ResponseRoute::ManualSearch: return "manual-search";
        case ResponseRoute::SmallModel: return "small-model";
        case ResponseRoute::LargeModel: return "large-model";
        case ResponseRoute::Defer: return "defer";
    }
    return "unknown";
}

}  // namespace offgrid


#pragma once

#include <cstddef>
#include <string>

namespace offgrid {

enum class ResponseRoute {
    StaticCard,
    ManualSearch,
    SmallModel,
    LargeModel,
    Defer
};

struct InferenceProfile {
    std::string name;
    double system_watts{};
    double tokens_per_second{};
    double fixed_seconds{};
};

struct EnergyEstimate {
    double runtime_seconds{};
    double watt_hours{};
    double milliwatt_hours_per_token{};
    double solar_recharge_minutes{};
};

struct PowerSnapshot {
    double usable_watt_hours{};
    double reserve_watt_hours{};
    double net_solar_watts{};
};

struct RouteDecision {
    ResponseRoute route{ResponseRoute::Defer};
    EnergyEstimate estimate{};
    std::string reason;
};

EnergyEstimate estimate_query_cost(
    const InferenceProfile& profile,
    std::size_t input_tokens,
    std::size_t output_tokens,
    double net_solar_watts);

RouteDecision choose_route(
    bool reviewed_card_hit,
    bool manual_search_hit,
    const PowerSnapshot& power,
    const InferenceProfile& small_model,
    const InferenceProfile& large_model,
    std::size_t input_tokens,
    std::size_t output_tokens);

const char* route_name(ResponseRoute route);

}  // namespace offgrid


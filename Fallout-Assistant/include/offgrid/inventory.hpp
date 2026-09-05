#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

struct InventoryItem {
    std::string id;
    std::string category;
    std::string name;
    double quantity{};
    std::string unit;
    double low_threshold{};
    std::string note;
};

struct InventoryState {
    bool simulation{true};
    std::string coordinates{"UNKNOWN / SIMULATED"};
    double health_percent{92.0};
    double battery_percent{78.0};
    double solar_watts{42.0};
    double battery_voltage{12.7};
    double temperature_c{18.0};
    double humidity_percent{52.0};
    double radiation_msv_h{0.04};
    std::string updated_at;
    std::vector<InventoryItem> items;
};

struct InventorySummary {
    double potable_water_liters{};
    double food_meals{};
    double medkits{};
    double bandages{};
    double ammo_rounds{};
    double battery_cells{};
    double charged_battery_cells{};
    double shelter_units{};
    std::size_t supply_item_types{};
    int readiness_percent{};
    std::string generated_at;
    std::vector<std::string> alerts;
};

class InventoryStore {
public:
    explicit InventoryStore(std::filesystem::path path);

    const std::filesystem::path& path() const;
    bool load(InventoryState& state, std::string& error) const;
    bool save(InventoryState& state, std::string& error) const;
    bool write_summary(
        const InventoryState& state, const InventorySummary& summary,
        std::string& error) const;
    std::filesystem::path summary_path() const;

private:
    std::filesystem::path path_;
};

InventoryState default_inventory_state();
InventorySummary summarize_inventory(const InventoryState& state);
double inventory_quantity(const InventoryState& state, std::string_view id);
std::string inventory_summary_text(
    const InventoryState& state, const InventorySummary& summary);
std::filesystem::path inventory_path();

}  // namespace offgrid

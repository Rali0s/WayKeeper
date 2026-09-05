#include "offgrid/inventory.hpp"

#include "offgrid/profile.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace offgrid {
namespace {

constexpr std::string_view magic = "WAYKEEPER-INVENTORY-1";

std::string local_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    std::ostringstream output;
    output << std::put_time(&local, "%Y-%m-%d %H:%M:%S %z");
    return output.str();
}

std::string clean_field(std::string value, const std::size_t limit = 160) {
    value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char character) {
        return character == '\n' || character == '\r' || character == '\0' ||
               (character < 0x20 && character != '\t');
    }), value.end());
    std::replace(value.begin(), value.end(), '\t', ' ');
    if (value.size() > limit) value.resize(limit);
    return value;
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (;;) {
        const auto end = line.find('\t', start);
        fields.push_back(line.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return fields;
}

bool parse_number(const std::string& value, double& output) {
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed)) return false;
        output = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

void add_alerts(const InventoryState& state, InventorySummary& summary) {
    for (const auto& item : state.items) {
        if (item.quantity <= item.low_threshold) {
            summary.alerts.push_back(
                "LOW " + item.category + " // " + item.name + " " +
                std::to_string(static_cast<int>(std::round(item.quantity))) + " " + item.unit);
        }
    }
    if (state.battery_percent <= 25.0) summary.alerts.emplace_back("LOW POWER // BATTERY AT OR BELOW 25%");
    if (state.health_percent <= 50.0) summary.alerts.emplace_back("OPERATOR HEALTH BELOW 50% // SIMULATION");
    if (state.radiation_msv_h >= 0.5) summary.alerts.emplace_back("ELEVATED RADIATION SENSOR // SIMULATION");
    if (state.coordinates.empty() || state.coordinates == "UNKNOWN / SIMULATED") {
        summary.alerts.emplace_back("COORDINATES NOT SET");
    }
}

}  // namespace

InventoryState default_inventory_state() {
    InventoryState state;
    state.updated_at = local_timestamp();
    state.items = {
        {"potable-water", "H2O", "Potable Water", 12.0, "L", 4.0, "SIMULATED potable reserve"},
        {"purification-tablets", "H2O", "Purification Tablets", 20.0, "tablets", 8.0, "Follow the exact product label"},
        {"ration-meals", "FOOD", "Ration Meals", 9.0, "meals", 3.0, "SIMULATED shelf-stable meals"},
        {"energy-bars", "FOOD", "Energy Bars", 6.0, "bars", 2.0, "SIMULATED quick calories"},
        {"medkits", "MEDICAL", "MedKits", 2.0, "kits", 1.0, "SIMULATED sealed kits"},
        {"bandages", "MEDICAL", "Bandages", 12.0, "dressings", 4.0, "SIMULATED sterile dressings"},
        {"utility-cord", "SUPPLIES", "Utility Cord", 25.0, "m", 8.0, "SIMULATED general cordage"},
        {"repair-tape", "SUPPLIES", "Repair Tape", 2.0, "rolls", 1.0, "SIMULATED repair stock"},
        {"emergency-tarp", "SHELTER", "Emergency Tarp", 1.0, "unit", 0.0, "SIMULATED shelter layer"},
        {"thermal-blankets", "SHELTER", "Thermal Blankets", 3.0, "blankets", 1.0, "SIMULATED thermal reserve"},
        {"rifle-ammunition", "AMMO", "Rifle Ammunition", 60.0, "rounds", 20.0, "SIMULATED count only"},
        {"handgun-ammunition", "AMMO", "Handgun Ammunition", 24.0, "rounds", 12.0, "SIMULATED count only"},
        {"battery-cells", "POWER", "Rechargeable Battery Cells", 8.0, "cells", 4.0, "SIMULATED common cells"},
        {"charged-battery-cells", "POWER", "Charged Battery Cells", 6.0, "cells", 3.0, "SIMULATED cells ready"},
    };
    return state;
}

double inventory_quantity(const InventoryState& state, const std::string_view id) {
    const auto found = std::find_if(state.items.begin(), state.items.end(), [&](const auto& item) {
        return item.id == id;
    });
    return found == state.items.end() ? 0.0 : found->quantity;
}

InventorySummary summarize_inventory(const InventoryState& state) {
    InventorySummary summary;
    summary.potable_water_liters = inventory_quantity(state, "potable-water");
    summary.food_meals = inventory_quantity(state, "ration-meals");
    summary.medkits = inventory_quantity(state, "medkits");
    summary.bandages = inventory_quantity(state, "bandages");
    summary.battery_cells = inventory_quantity(state, "battery-cells");
    summary.charged_battery_cells = inventory_quantity(state, "charged-battery-cells");
    for (const auto& item : state.items) {
        if (item.category == "AMMO") summary.ammo_rounds += item.quantity;
        if (item.category == "SHELTER") summary.shelter_units += item.quantity;
        if (item.category == "SUPPLIES" && item.quantity > 0.0) ++summary.supply_item_types;
    }
    add_alerts(state, summary);

    double readiness = std::clamp(state.health_percent, 0.0, 100.0) * 0.35;
    readiness += std::min(1.0, summary.potable_water_liters / 12.0) * 15.0;
    readiness += std::min(1.0, summary.food_meals / 9.0) * 10.0;
    readiness += std::min(1.0, summary.medkits / 2.0) * 10.0;
    readiness += std::min(1.0, summary.charged_battery_cells / 6.0) * 10.0;
    readiness += std::min(1.0, summary.shelter_units / 4.0) * 10.0;
    readiness += std::clamp(state.battery_percent, 0.0, 100.0) * 0.10;
    readiness -= std::min<std::size_t>(summary.alerts.size(), 4) * 2.0;
    summary.readiness_percent = static_cast<int>(std::round(std::clamp(readiness, 0.0, 100.0)));
    summary.generated_at = local_timestamp();
    return summary;
}

InventoryStore::InventoryStore(std::filesystem::path path) : path_(std::move(path)) {}

const std::filesystem::path& InventoryStore::path() const { return path_; }

bool InventoryStore::load(InventoryState& state, std::string& error) const {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        error = "No inventory state exists yet.";
        return false;
    }
    std::string line;
    if (!std::getline(input, line) || line != magic) {
        error = "Unsupported inventory state format: " + path_.string();
        return false;
    }
    InventoryState loaded;
    loaded.items.clear();
    while (std::getline(input, line)) {
        const auto fields = split_tabs(line);
        if (fields.empty()) continue;
        if (fields[0] == "META" && fields.size() >= 3) {
            const auto& key = fields[1];
            const auto& value = fields[2];
            if (key == "simulation") loaded.simulation = value != "0";
            else if (key == "coordinates") loaded.coordinates = value;
            else if (key == "updated_at") loaded.updated_at = value;
        } else if (fields[0] == "SENSOR" && fields.size() >= 3) {
            double value = 0.0;
            if (!parse_number(fields[2], value)) continue;
            if (fields[1] == "health_percent") loaded.health_percent = value;
            else if (fields[1] == "battery_percent") loaded.battery_percent = value;
            else if (fields[1] == "solar_watts") loaded.solar_watts = value;
            else if (fields[1] == "battery_voltage") loaded.battery_voltage = value;
            else if (fields[1] == "temperature_c") loaded.temperature_c = value;
            else if (fields[1] == "humidity_percent") loaded.humidity_percent = value;
            else if (fields[1] == "radiation_msv_h") loaded.radiation_msv_h = value;
        } else if (fields[0] == "ITEM" && fields.size() >= 8) {
            InventoryItem item;
            item.id = fields[1];
            item.category = fields[2];
            item.name = fields[3];
            item.unit = fields[5];
            item.note = fields[7];
            if (!parse_number(fields[4], item.quantity) ||
                !parse_number(fields[6], item.low_threshold) || item.id.empty()) continue;
            loaded.items.push_back(std::move(item));
        }
    }
    if (loaded.items.empty()) {
        error = "Inventory state contains no item records.";
        return false;
    }
    state = std::move(loaded);
    error.clear();
    return true;
}

bool InventoryStore::save(InventoryState& state, std::string& error) const {
    state.simulation = true;
    state.updated_at = local_timestamp();
    std::error_code filesystem_error;
    std::filesystem::create_directories(path_.parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create inventory state directory: " + filesystem_error.message();
        return false;
    }
    std::ofstream output(path_, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not write inventory state: " + path_.string();
        return false;
    }
    output << magic << '\n'
           << "META\tsimulation\t1\n"
           << "META\tcoordinates\t" << clean_field(state.coordinates) << '\n'
           << "META\tupdated_at\t" << clean_field(state.updated_at) << '\n'
           << std::setprecision(12)
           << "SENSOR\thealth_percent\t" << state.health_percent << '\n'
           << "SENSOR\tbattery_percent\t" << state.battery_percent << '\n'
           << "SENSOR\tsolar_watts\t" << state.solar_watts << '\n'
           << "SENSOR\tbattery_voltage\t" << state.battery_voltage << '\n'
           << "SENSOR\ttemperature_c\t" << state.temperature_c << '\n'
           << "SENSOR\thumidity_percent\t" << state.humidity_percent << '\n'
           << "SENSOR\tradiation_msv_h\t" << state.radiation_msv_h << '\n';
    for (const auto& item : state.items) {
        output << "ITEM\t" << clean_field(item.id, 64)
               << '\t' << clean_field(item.category, 32)
               << '\t' << clean_field(item.name)
               << '\t' << std::max(0.0, item.quantity)
               << '\t' << clean_field(item.unit, 32)
               << '\t' << std::max(0.0, item.low_threshold)
               << '\t' << clean_field(item.note, 240) << '\n';
    }
    if (!output) {
        error = "Inventory state write was incomplete: " + path_.string();
        return false;
    }
    error.clear();
    return true;
}

std::filesystem::path InventoryStore::summary_path() const {
    return path_.parent_path() / "inventory-summary.txt";
}

bool InventoryStore::write_summary(
    const InventoryState& state, const InventorySummary& summary, std::string& error) const {
    std::error_code filesystem_error;
    std::filesystem::create_directories(summary_path().parent_path(), filesystem_error);
    if (filesystem_error) {
        error = "Could not create inventory summary directory: " + filesystem_error.message();
        return false;
    }
    std::ofstream output(summary_path(), std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Could not write inventory summary: " + summary_path().string();
        return false;
    }
    output << inventory_summary_text(state, summary);
    if (!output) {
        error = "Inventory summary write was incomplete: " + summary_path().string();
        return false;
    }
    error.clear();
    return true;
}

std::string inventory_summary_text(
    const InventoryState& state, const InventorySummary& summary) {
    std::ostringstream output;
    output << "WAYKEEPER INVENTORY CLOSE SUMMARY\n"
           << "=================================\n"
           << "MODE          SIMULATION DATA ONLY\n"
           << "GENERATED     " << summary.generated_at << '\n'
           << "COORDINATES   " << state.coordinates << '\n'
           << "READINESS     " << summary.readiness_percent << "%\n"
           << "HEALTH        " << state.health_percent << "%\n"
           << "H2O           " << summary.potable_water_liters << " L POTABLE\n"
           << "FOOD          " << summary.food_meals << " MEALS\n"
           << "MEDICAL       " << summary.medkits << " MEDKITS / "
           << summary.bandages << " DRESSINGS\n"
           << "SUPPLIES      " << summary.supply_item_types << " STOCKED ITEM TYPES\n"
           << "SHELTER       " << summary.shelter_units << " TRACKED UNITS\n"
           << "AMMO          " << summary.ammo_rounds << " ROUNDS (COUNT ONLY)\n"
           << "BATTERY CELL  " << summary.charged_battery_cells << " CHARGED / "
           << summary.battery_cells << " TOTAL\n"
           << "POWER         " << state.battery_percent << "% / "
           << state.battery_voltage << " V / SOLAR " << state.solar_watts << " W\n"
           << "SENSORS       " << state.temperature_c << " C / "
           << state.humidity_percent << "% RH / " << state.radiation_msv_h << " mSv/h\n"
           << "ALERTS        " << summary.alerts.size() << '\n';
    if (summary.alerts.empty()) output << "              NONE IN SIMULATION DATA\n";
    else for (const auto& alert : summary.alerts) output << "              " << alert << '\n';
    output << "\nITEM REGISTER\n-------------\n";
    for (const auto& item : state.items) {
        output << std::left << std::setw(12) << item.category << " | "
               << std::setw(28) << item.name << " | "
               << item.quantity << ' ' << item.unit << " | LOW " << item.low_threshold;
        if (!item.note.empty()) output << " | " << item.note;
        output << '\n';
    }
    return output.str();
}

std::filesystem::path inventory_path() {
    return profile_path().parent_path() / "inventory.tsv";
}

}  // namespace offgrid

#include "offgrid/energy.hpp"
#include "offgrid/guide.hpp"
#include "offgrid/herbs.hpp"
#include "offgrid/image.hpp"
#include "offgrid/inventory.hpp"
#include "offgrid/io_scout.hpp"
#include "offgrid/journal.hpp"
#include "offgrid/library.hpp"
#include "offgrid/map.hpp"
#include "offgrid/map_annotations.hpp"
#include "offgrid/mesh.hpp"
#include "offgrid/network.hpp"
#include "offgrid/ollama.hpp"
#include "offgrid/profile.hpp"
#include "offgrid/schematics.hpp"
#include "offgrid/sentinel.hpp"
#include "offgrid/settings.hpp"
#include "offgrid/telemetry.hpp"
#include "offgrid/terminal.hpp"
#include "offgrid/unlocks.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace {

bool near(const double left, const double right, const double tolerance = 0.0001) {
    return std::abs(left - right) <= tolerance;
}

void require(const bool condition, const char* message) {
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

}  // namespace

int main() {
    const auto terminal = offgrid::terminal_size();
    require(terminal.columns > 0 && terminal.rows > 0, "terminal geometry fallback");
    require(offgrid::valid_terminal_resolution(800, 600), "default terminal resolution");
    require(!offgrid::valid_terminal_resolution(320, 200), "reject unusable terminal resolution");
    require(offgrid::valid_terminal_theme("blue") &&
                offgrid::valid_terminal_theme("gold") &&
                offgrid::valid_terminal_theme("green"),
            "embedded terminal themes");
    require(!offgrid::valid_terminal_theme("rainbow"), "reject unsupported terminal theme");
    require(offgrid::valid_terminal_layout("workstation") &&
                offgrid::valid_terminal_layout("minimal") &&
                offgrid::valid_terminal_layout("static"),
            "terminal layout modes");
    require(!offgrid::valid_terminal_layout("gui"), "reject unsupported terminal layout");
    require(offgrid::valid_companion_render("auto") &&
                offgrid::valid_companion_render("ansi") &&
                offgrid::valid_companion_render("off"),
            "companion render modes");
    require(!offgrid::valid_companion_render("popup"), "reject popup companion render");
    require(offgrid::valid_room_code("WK-01") && !offgrid::valid_room_code("ROOM 01"),
            "room code validation");
    require(offgrid::sentinel_idle_timeout_seconds == 300,
            "Sentinel dead-man timeout is five minutes");
    require(offgrid::valid_sentinel_movie_speed(0.5) &&
                offgrid::valid_sentinel_movie_speed(1.0) &&
                offgrid::valid_sentinel_movie_speed(2.0) &&
                !offgrid::valid_sentinel_movie_speed(3.0),
            "Sentinel movie speed presets");
    require(offgrid::next_sentinel_movie_speed(0.5) == 1.0 &&
                offgrid::next_sentinel_movie_speed(1.0) == 2.0 &&
                offgrid::next_sentinel_movie_speed(2.0) == 0.5,
            "Sentinel movie speed cycle");
    require(offgrid::sentinel_card_duration(0.5) == std::chrono::seconds(24) &&
                offgrid::sentinel_card_duration(1.0) == std::chrono::seconds(12) &&
                offgrid::sentinel_card_duration(2.0) == std::chrono::seconds(6),
            "Sentinel card timing stays below dead-man window");
    offgrid::SentinelEscapeSequence sentinel_wake;
    require(!sentinel_wake.push("escape") && !sentinel_wake.push("w") &&
                !sentinel_wake.push("k") && sentinel_wake.push(""),
            "Sentinel wake chord Escape-W-K-Enter");
    require(!sentinel_wake.push("escape") && !sentinel_wake.push("x") &&
                !sentinel_wake.push("w") && !sentinel_wake.push("k") &&
                !sentinel_wake.push(""),
            "Sentinel wake chord rejects broken sequence");
    require(sentinel_wake.push("\x1bWK\r"),
            "Sentinel wake chord accepts coalesced terminal bytes");
    const auto sentinel_test_root = std::filesystem::temp_directory_path() /
        ("offgrid-sentinel-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto last_will_path = sentinel_test_root / "last-will.txt";
    std::string last_will_error;
    require(offgrid::save_sentinel_last_will(
                last_will_path, "If found, carry the archive home.\nRemember us.",
                last_will_error),
            "Sentinel Last Will local save");
    const auto last_will = offgrid::load_sentinel_last_will(
        last_will_path, last_will_error);
    require(last_will && last_will->find("Remember us.") != std::string::npos,
            "Sentinel Last Will local load");
    require(!offgrid::save_sentinel_last_will(
                last_will_path, std::string("unsafe\x1bmessage", 14), last_will_error),
            "Sentinel Last Will rejects terminal controls");
    std::error_code sentinel_cleanup_error;
    std::filesystem::remove_all(sentinel_test_root, sentinel_cleanup_error);
    const offgrid::OllamaClient embedded_ollama;
    require(!embedded_ollama.enabled(), "Ollama compile-disabled by default");
    require(!embedded_ollama.server_ready(), "disabled Ollama never probes loopback");

    const auto schematic_test_root = std::filesystem::temp_directory_path() /
        ("offgrid-schematic-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(schematic_test_root);
    {
        std::ofstream diagram(schematic_test_root / "aligned.txt", std::ios::binary);
        diagram << "A\tB  \n  C\n";
        std::ofstream catalog(schematic_test_root / "catalog.tsv");
        catalog << "id\ttitle\tcategory\tsource_note\tsafety_note\tfile\n"
                << "aligned\tAligned Diagram\ttest\tlocal\tfixture\taligned.txt\n";
    }
    std::vector<offgrid::TextSchematic> schematics;
    std::string schematic_error;
    require(offgrid::load_text_schematics(
                schematic_test_root / "catalog.tsv", schematics, schematic_error) &&
                schematics.size() == 1,
            "text schematic catalog load");
    const auto schematic_source = offgrid::read_text_schematic(
        schematics.front(), schematic_error);
    require(schematic_source.has_value(), "text schematic source read");
    const auto schematic_lines = offgrid::fixed_schematic_lines(*schematic_source);
    require(schematic_lines.size() == 2 && schematic_lines[0] == "A       B  " &&
                schematic_lines[1] == "  C",
            "text schematic preserves tabs and blank cells");
    require(offgrid::fixed_schematic_width(schematic_lines) == 11,
            "text schematic fixed width");
    require(offgrid::fixed_schematic_slice(schematic_lines[1], 0, 6) == "  C   ",
            "text schematic slice preserves leading and trailing blanks");
    std::error_code schematic_cleanup_error;
    std::filesystem::remove_all(schematic_test_root, schematic_cleanup_error);

    const auto journal_test_root = std::filesystem::temp_directory_path() /
        ("offgrid-journal-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    offgrid::JournalStore journal(journal_test_root);
    offgrid::JournalEntry log;
    log.kind = "CAPTAINS_LOG";
    log.title = "Weather watch";
    log.tags = "weather, watch";
    log.operator_name = "Test Operator";
    log.incident = "Test / Simulation";
    log.terrain = "Coastal / Marine";
    log.sleep_hours = 6.5;
    log.miles_traveled = 4.25;
    log.health_note = "Feet checked; hydration nominal.";
    log.body = "Barometer falling. Secure loose gear.";
    std::string journal_error;
    require(journal.create(log, journal_error), "journal entry create");
    require(!log.id.empty() && !log.created_at.empty(), "journal metadata generation");
    require(journal.entries(journal_error).size() == 1, "journal entry list");
    require(journal.search("barometer", journal_error).size() == 1, "journal full-text search");
    log.title = "Weather watch updated";
    log.body += " Recheck in one hour.";
    require(journal.update(log, journal_error), "journal entry update");
    const auto updated_log = journal.find(log.id, false, journal_error);
    require(updated_log && updated_log->body.find("one hour") != std::string::npos,
            "journal updated body read");
    require(updated_log && near(updated_log->sleep_hours, 6.5) &&
                near(updated_log->miles_traveled, 4.25),
            "Captain's Log health metrics persist");
    require(journal.search("hydration nominal", journal_error).size() == 1,
            "Captain's Log health-note search");
    require(journal.archive(log.id, journal_error), "journal archive");
    require(journal.entries(journal_error).empty(), "journal active list after archive");
    require(journal.archived_entries(journal_error).size() == 1, "journal archived list");
    require(journal.restore(log.id, journal_error), "journal restore");
    require(journal.entries(journal_error).size() == 1, "journal active list after restore");
    offgrid::JournalEntry citizen;
    citizen.kind = "CITIZEN";
    citizen.title = "Test Resident";
    citizen.tags = "citizen,resident";
    citizen.operator_name = "Test Operator";
    citizen.incident = "Test / Simulation";
    citizen.terrain = "Camp";
    citizen.body = "STATUS: RESIDENT\nBLOOD TYPE: UNKNOWN\nALLERGIES: UNKNOWN";
    require(journal.create(citizen, journal_error), "citizen registry record create");
    require(journal.search("Test Resident", journal_error).size() == 1,
            "citizen registry title search");

    const auto inventory_test_path = journal_test_root / "inventory.tsv";
    offgrid::InventoryStore inventory_store(inventory_test_path);
    auto inventory = offgrid::default_inventory_state();
    inventory.coordinates = "42.6526 N / 73.7562 W // SIM";
    std::string inventory_error;
    require(inventory_store.save(inventory, inventory_error), "inventory state save");
    offgrid::InventoryState loaded_inventory;
    require(inventory_store.load(loaded_inventory, inventory_error), "inventory state load");
    require(near(offgrid::inventory_quantity(loaded_inventory, "potable-water"), 12.0),
            "inventory named quantity");
    require(loaded_inventory.coordinates == inventory.coordinates, "inventory coordinates persist");
    const auto inventory_summary = offgrid::summarize_inventory(loaded_inventory);
    require(inventory_summary.readiness_percent > 0, "inventory readiness summary");
    require(near(inventory_summary.ammo_rounds, 84.0), "inventory ammo count summary");
    require(inventory_store.write_summary(
                loaded_inventory, inventory_summary, inventory_error),
            "inventory close summary write");
    require(std::filesystem::exists(inventory_store.summary_path()),
            "inventory close summary artifact");
#ifdef _WIN32
    _putenv_s("OFFGRID_STATE_DIR", journal_test_root.string().c_str());
#else
    setenv("OFFGRID_STATE_DIR", journal_test_root.string().c_str(), 1);
#endif
    offgrid::MeshPowerMode mesh_mode;
    require(offgrid::parse_mesh_power_mode("eco", mesh_mode) &&
                mesh_mode == offgrid::MeshPowerMode::Eco,
            "mesh power mode parse");
    require(offgrid::valid_mesh_nickname("WAYKEEPER-07") &&
                !offgrid::valid_mesh_nickname("WAY KEEPER"),
            "mesh nickname validation");
    offgrid::MeshProfile mesh_profile;
    mesh_profile.nickname = "WAYKEEPER-07";
    mesh_profile.requested_mode = offgrid::MeshPowerMode::Eco;
    const auto mesh_actions = offgrid::mesh_sidebar_actions(mesh_profile);
    require(mesh_actions.size() == 5, "BLE sidebar has five fixed items");
    require(mesh_actions[3].starts_with("CHAT") && mesh_actions[4] == "BACK",
            "BLE sidebar item 4 chat and item 5 back");
    std::string mesh_error;
    require(offgrid::save_mesh_profile(mesh_profile, mesh_error),
            "mesh profile save");
    offgrid::MeshProfile loaded_mesh;
    require(offgrid::load_mesh_profile(loaded_mesh, mesh_error) &&
                loaded_mesh.nickname == mesh_profile.nickname &&
                loaded_mesh.requested_mode == offgrid::MeshPowerMode::Eco,
            "mesh profile load");
    const auto mesh_readiness = offgrid::inspect_mesh_readiness();
    require(!mesh_readiness.protocol_backend_ready,
            "mesh transmit remains locked before interoperability gate");
    require(offgrid::wipe_mesh_state(mesh_error) &&
                !std::filesystem::exists(offgrid::mesh_profile_path()),
            "mesh panic wipe removes bounded state");
    offgrid::ScoutTransport scout_transport;
    offgrid::ScoutAdapter scout_adapter;
    require(offgrid::parse_scout_transport("usb-serial", scout_transport) &&
                scout_transport == offgrid::ScoutTransport::Serial,
            "UART Scout transport aliases");
    require(offgrid::parse_scout_adapter("ogp1", scout_adapter) &&
                scout_adapter == offgrid::ScoutAdapter::SensorOgp1,
            "UART Scout adapter aliases");
    offgrid::ScoutProfile scout_profile;
    scout_profile.name = "TEST-RIG";
    scout_profile.transport = offgrid::ScoutTransport::Serial;
    scout_profile.adapter = offgrid::ScoutAdapter::SensorOgp1;
#ifdef _WIN32
    scout_profile.endpoint = "COM99";
#else
    scout_profile.endpoint = "/dev/ttyTEST";
#endif
    scout_profile.baud = 115200;
    scout_profile.manual_query = "test equipment service manual";
    std::string scout_error;
    require(offgrid::validate_scout_profile(scout_profile, scout_error),
            "UART Scout serial profile validation");
    scout_profile.allow_transmit = true;
    require(!offgrid::validate_scout_profile(scout_profile, scout_error),
            "UART Scout rejects TX without authorization");
    scout_profile.authorized = true;
    require(offgrid::save_scout_profile(scout_profile, scout_error),
            "UART Scout profile persistence");
    offgrid::ScoutProfile loaded_scout;
    require(offgrid::load_scout_profile(loaded_scout, scout_error) &&
                loaded_scout.name == scout_profile.name &&
                loaded_scout.adapter == offgrid::ScoutAdapter::SensorOgp1 &&
                loaded_scout.allow_transmit,
            "UART Scout profile round trip");
    require(offgrid::scout_baud_rates().size() >= 10,
            "UART Scout field baud catalog");
    const auto ghostline_capture = journal_test_root / "ghostline" / "field-io.glcap";
    std::filesystem::create_directories(ghostline_capture.parent_path());
    {
        std::ofstream output(ghostline_capture);
        output << "# GLCAP1\tunix_ns\tflow\tdirection\ttransport\tbytes\tsummary\thex\tascii\n"
               << "GLCAP1\t1786933504107691000\t1\ts2c\ttcp\t4\t"
                  "MQTT CONNACK server->client reason=success\t20020000\t ...\n";
    }
    std::string ghostline_render;
    require(offgrid::render_ghostline_capture(
                ghostline_capture, 8, ghostline_render, scout_error) &&
                ghostline_render.find("MQTT CONNACK") != std::string::npos &&
                ghostline_render.find("20 02 00 00") != std::string::npos,
            "WayKeeper Ghostline terminal capture reader");
    const auto map_overlay_path = journal_test_root / "map-overlays" / "ny.tsv";
    offgrid::MapAnnotationStore map_store(map_overlay_path);
    offgrid::MapAnnotations map_annotations;
    map_annotations.markers = {
        {"1", 42.65, -73.75, "Homestead", "Base Camp"},
        {"2", 42.70, -73.70, "Water", "North Spring"}};
    std::string map_store_error;
    require(map_store.save(map_annotations, map_store_error), "map annotations save");
    offgrid::MapAnnotations loaded_annotations;
    require(map_store.load(loaded_annotations, map_store_error), "map annotations load");
    require(loaded_annotations.markers.size() == 2 &&
                loaded_annotations.markers[1].type == "Water",
            "map annotation round trip");
    require(offgrid::waypoint_distance_miles(loaded_annotations.markers) > 4.0,
            "map waypoint haversine mileage");
    require(offgrid::map_marker_symbol("Abandoned Structure") == 'A',
            "map marker type symbol");
    const auto network = offgrid::inspect_network();
    require(!network.online || (!network.interface_name.empty() && !network.address.empty()),
            "network status interface metadata");
    const auto unlock_path = journal_test_root / "next-boot-unlock.ini";
    std::string unlock_error;
    require(offgrid::schedule_boot_unlock(
                unlock_path, offgrid::BootUnlock::Raider, unlock_error),
            "Raider boot unlock schedule");
    require(offgrid::consume_boot_unlock(unlock_path, unlock_error) ==
                offgrid::BootUnlock::Raider,
            "Raider boot unlock consume");
    require(!std::filesystem::exists(unlock_path), "boot unlock one-shot removal");
    require(offgrid::schedule_boot_unlock(
                unlock_path, offgrid::BootUnlock::Raider, unlock_error),
            "initial boot unlock schedule");
    require(offgrid::schedule_boot_unlock(
                unlock_path, offgrid::BootUnlock::VaultBlueRaider, unlock_error),
            "newest boot unlock overwrites prior state");
    require(offgrid::consume_boot_unlock(unlock_path, unlock_error) ==
                offgrid::BootUnlock::VaultBlueRaider,
            "Vault-Blue Raider boot unlock precedence");
    std::error_code journal_cleanup_error;
    std::filesystem::remove_all(journal_test_root, journal_cleanup_error);

    const offgrid::InferenceProfile small{"small", 10.0, 10.0, 2.0};
    const auto cost = offgrid::estimate_query_cost(small, 100, 100, 20.0);
    require(near(cost.runtime_seconds, 22.0), "query runtime estimate");
    require(near(cost.watt_hours, 10.0 * 22.0 / 3600.0), "query Wh estimate");
    require(near(cost.solar_recharge_minutes, cost.watt_hours / 20.0 * 60.0), "solar payback");

    const offgrid::InferenceProfile large{"large", 25.0, 5.0, 3.0};
    const offgrid::PowerSnapshot low_power{5.0, 5.0, 0.0};
    const auto card_route = offgrid::choose_route(
        true, false, low_power, small, large, 100, 100);
    require(card_route.route == offgrid::ResponseRoute::StaticCard, "static card route");

    const auto defer_route = offgrid::choose_route(
        false, false, low_power, small, large, 100, 100);
    require(defer_route.route == offgrid::ResponseRoute::Defer, "battery reserve route");

    const auto water = offgrid::find_reviewed_card(
        "How long should I leave water purification tablets in for?");
    require(water.has_value(), "water tablet card match");
    require(water && water->answer.find("no safe universal tablet time") != std::string::npos,
            "water tablet safety language");
    require(offgrid::find_reviewed_card("How do I boil water?").has_value(), "boiling card match");
    require(offgrid::find_reviewed_card("What should I do after radioactive fallout?").has_value(),
            "radiation card match");

    std::string candidate_error;
    const auto candidates = offgrid::load_candidate_cards(
        offgrid::resource_root() / "cards" / "candidates" / "phase-1", candidate_error);
    require(candidate_error.empty(), "Phase 1 candidate-card load");
    require(candidates.size() == 25, "Phase 1 candidate-card count");
    require(std::count_if(candidates.begin(), candidates.end(), [](const auto& card) {
                return card.risk == "high";
            }) == 8,
            "Phase 1 high-risk count");
    const auto shelter_candidate = std::find_if(
        candidates.begin(), candidates.end(), [](const auto& card) {
            return card.id == "chemical.shelter.hvac-shutdown";
        });
    require(shelter_candidate != candidates.end(), "candidate metadata ID");
    require(shelter_candidate != candidates.end() &&
                shelter_candidate->source_pages == "2" &&
                shelter_candidate->answer.find("outside doors") != std::string::npos &&
                shelter_candidate->limits.find("outdoor chemical release") != std::string::npos,
            "candidate source and safety fields");
    require(!offgrid::find_reviewed_card(
                 "Should I stop outside air during chemical sheltering?").has_value(),
            "candidate remains isolated from reviewed Guide routing");

    require(offgrid::incident_options().size() >= 10, "incident onboarding options");
    require(offgrid::terrain_options().size() >= 9, "terrain onboarding options");
    require(offgrid::OperatorProfile{"Operator", "Test / Simulation", "Urban"}.simulation(),
            "test profile simulation flag");
    require(offgrid::OperatorProfile{
        "Operator", "Zombie / Fictional Drill", "Suburban"}.simulation(),
        "fictional profile simulation flag");
    require(!offgrid::OperatorProfile{
        "Operator", "Nuclear / Radiological", "Urban"}.simulation(),
        "active incident profile flag");

    offgrid::SurvivalLibrary library;
    std::string library_error;
    require(library.load(offgrid::resource_root() / "library" / "catalog.tsv", library_error),
            "library catalog load");
    require(library.documents().size() >= 295, "expanded library document count");
    std::size_t agriculture_documents = 0;
    std::size_t agriculture_pages = 0;
    std::set<std::string> agriculture_shelves;
    for (const auto& document : library.documents()) {
        if (!document.category.starts_with("Agriculture-")) continue;
        ++agriculture_documents;
        agriculture_pages += document.pages;
        agriculture_shelves.insert(document.category);
    }
    require(agriculture_documents == 26, "agriculture PDF collection size");
    require(agriculture_pages == 3291, "agriculture page count");
    require(agriculture_shelves.size() == 15, "all agriculture shelves populated");
    const auto cookbook_document = std::find_if(
        library.documents().begin(), library.documents().end(), [](const auto& document) {
            return document.id == "anarchists-cookbook-iv-4-14-quarantine";
        });
    require(cookbook_document != library.documents().end(), "restricted Cookbook shelf installed");
    if (cookbook_document != library.documents().end()) {
        require(cookbook_document->category == "Cookbook-Underground-Restricted",
                "Cookbook quarantine category");
        const auto cookbook_index = static_cast<std::size_t>(
            std::distance(library.documents().begin(), cookbook_document));
        const auto notice = library.read_page(cookbook_index, 1);
        require(notice && notice->find("AGE RESTRICTION: 18+ ONLY") != std::string::npos &&
                    notice->find("ILLEGAL CONTENT - FOR REFERENCE PURPOSES ONLY - HISTORIC") !=
                        std::string::npos,
                "Cookbook age gate and historic-reference notice");
        require(!library.search_document(
                    cookbook_index, "local restricted search", 5).empty(),
                "Cookbook gated local text index");
        const auto cookbook_hits = library.search("anarchists cookbook", 100);
        require(std::none_of(
                    cookbook_hits.begin(), cookbook_hits.end(), [&](const auto& hit) {
                        return hit.document_index == cookbook_index;
                    }),
                "Cookbook excluded from Guide search");
    }
    const auto republic_document = std::find_if(
        library.documents().begin(), library.documents().end(), [](const auto& document) {
            return document.id == "plato-republic";
        });
    require(republic_document != library.documents().end(), "philosophy Republic reader installed");
    if (republic_document != library.documents().end()) {
        require(republic_document->category == "Philosophy-Government-Power",
                "philosophy reader shelf category");
        require(republic_document->pdf_path.extension() == ".txt",
                "philosophy uses plain-text reader adaptation");
        const auto first_page = library.read_page(
            static_cast<std::size_t>(std::distance(library.documents().begin(), republic_document)), 1);
        require(first_page && first_page->find("PUBLIC-DOMAIN FULL TEXT") != std::string::npos,
                "public-domain philosophy full-text status");
    }
    const auto hypothermia = library.search("how should I treat hypothermia", 5);
    require(!hypothermia.empty(), "ranked manual search");
    if (!hypothermia.empty()) {
        const auto page = library.read_page(
            hypothermia.front().document_index, hypothermia.front().page);
        require(page.has_value(), "manual page read");
    }
    require(!library.search("Cleaning Your Firearm", 5).empty(), "firearm care search");
    require(!library.search("Skinning Cased Fur", 5).empty(), "skinning search");
    require(!library.search("bow and drill fire", 5).empty(), "friction fire search");
    require(!library.search("living in wolf country", 5).empty(), "wolf safety search");
    require(!library.search("Debris Hut Shelter", 5).empty(), "improvised shelter search");
    require(!library.search("protecting log cabins from decay", 5).empty(),
            "log cabin maintenance search");
    require(!library.search("soil quality test kit", 5).empty(),
            "agriculture soil search");
    require(!library.search("seed storage", 5).empty(),
            "agriculture seed search");
    require(!library.search("rainwater harvesting", 5).empty(),
            "agriculture water search");
    require(!library.search("pasture condition scoring", 5).empty(),
            "agriculture pasture search");
    require(!library.search("commercial storage of fruits vegetables", 5).empty(),
            "agriculture post-harvest search");
    require(!library.search("permafrost foundation", 5).empty(),
            "cold climate shelter search");
    require(!library.search("In the beginning God created", 5).empty(),
            "offline Bible search");
    require(!library.search("Articles of Confederation and perpetual union", 5).empty(),
            "founding documents search");
    require(!library.search("official handbook of the Federal Government", 5).empty(),
            "government structure search");
    require(!library.search("coercive counterintelligence interrogation", 5).empty(),
            "declassified historical record search");
    require(!library.search("until philosophers are kings", 5).empty(),
            "Plato Republic full-text search");
    require(!library.search("FULL TEXT NOT INCLUDED LICENSE REQUIRED", 5).empty(),
            "licensed philosophy rights gate search");
    require(!library.search("method of sale", 5).empty(),
            "survival economy measurement search");
    require(!library.search("coincidence of wants", 5).empty(),
            "survival economy barter search");
    require(!library.search("community lifelines", 5).empty(),
            "rebuilding society search");
    require(!library.search("world of relentless surveillance", 5).empty(),
            "privacy and surveillance shelf search");
    require(!library.search("Cthulhu", 5).empty(),
            "fiction and occult literature shelf search");
    require(library.search("sinsemilla", 5).empty(),
            "hidden agriculture archive excluded from normal search");
    require(!library.search("SOURCE CLASS UNDERGROUND HISTORICAL UNVERIFIED", 5).empty(),
            "TEXTFILES underground warning search");

    if (offgrid::image_support_available()) {
        offgrid::ImageInfo image;
        std::string image_error;
        std::ostringstream image_output;
        require(offgrid::render_image_ansi(
                    offgrid::resource_root() / "maps" / "ny" /
                        "USGS-3DEP-New-York-State-hiking-trails-overview.png",
                    40, 10, false, image_output, image, image_error),
                "general ANSI image rendering");
        require(image.source_width > 0 && image.source_height > 0, "ANSI image source dimensions");
        const auto image_text = image_output.str();
        require(std::count(image_text.begin(), image_text.end(), '\n') == 10,
                "ANSI image viewport height");
        std::ostringstream truecolor_output;
        require(offgrid::render_image_ansi(
                    offgrid::resource_root().parent_path() / "RES" / "WayKeeper TM" /
                        "SurvivalMode.png",
                    30, 18, true, truecolor_output, image, image_error),
                "WayKeeper TrueColor mascot rendering");
        require(truecolor_output.str().find("\033[38;2;") != std::string::npos &&
                    truecolor_output.str().find("\033[48;2;") != std::string::npos,
                "24-bit ANSI foreground/background pixel output");
    }

    const auto reading = offgrid::parse_telemetry(
        "OGP1|solar|battery_voltage|13.42|V|1786766400000|measured");
    require(reading.has_value(), "telemetry parse");
    require(reading && reading->source == "solar", "telemetry source");
    require(reading && near(reading->value, 13.42), "telemetry value");
    require(reading && reading->quality == offgrid::MeasurementQuality::Measured,
            "telemetry quality");
    require(!offgrid::parse_telemetry("bad|line").has_value(), "reject malformed telemetry");

    if (offgrid::terrain_support_available()) {
        const auto maps = offgrid::discover_terrain_maps(offgrid::resource_root() / "maps");
        require(maps.size() >= 2, "independent state terrain-pack discovery");
        const auto ny_map = std::find_if(maps.begin(), maps.end(), [](const auto& path) {
            return path.parent_path().filename() == "ny";
        });
        const auto fl_map = std::find_if(maps.begin(), maps.end(), [](const auto& path) {
            return path.parent_path().filename() == "fl";
        });
        require(ny_map != maps.end(), "New York state pack discovery");
        require(fl_map != maps.end(), "Florida state pack discovery");
        offgrid::TerrainMapInfo terrain;
        std::string terrain_error;
        require(ny_map != maps.end() && offgrid::inspect_terrain(*ny_map, terrain, terrain_error),
                "GeoTIFF terrain inspection");
        require(terrain.source_width == 1200 && terrain.source_height == 800,
                "New York preview dimensions");
        require(terrain.minimum_elevation_m < 0.0 && terrain.maximum_elevation_m > 1400.0,
                "New York preview elevation range");
        require(terrain.trail_feature_count == 17392, "New York trail overlay discovery");
        require(terrain.rail_feature_count == 5560, "New York railroad overlay discovery");
        require(terrain.road_feature_count == 90, "New York primary-road overlay discovery");
        require(terrain.town_feature_count == 616, "New York town landmark discovery");
        require(terrain.river_feature_count == 1026, "New York major-river discovery");
        require(terrain.water_feature_count == 144, "New York water landmark discovery");
        const auto full_longitude_span = terrain.east - terrain.west;
        const auto full_latitude_span = terrain.north - terrain.south;
        std::ostringstream rendered;
        require(offgrid::render_terrain_ansi(
                    *ny_map, 40, 10, false, rendered, terrain, terrain_error),
                "plain-text terrain rendering");
        const auto terrain_text = rendered.str();
        require(std::count(terrain_text.begin(), terrain_text.end(), '\n') == 10,
                "terrain viewport height");
        require(terrain_text.find_first_of(".:-=+*#%@") != std::string::npos,
                "terrain relief content");

        offgrid::TerrainMapInfo centered_zoom;
        std::ostringstream centered_output;
        require(offgrid::render_terrain_ansi(
                    *ny_map, 40, 10, false, centered_output, centered_zoom,
                    terrain_error, offgrid::TerrainViewport{0.5, 0.5, 2.0}),
                "centered 2x terrain rendering");
        require(near(centered_zoom.east - centered_zoom.west, full_longitude_span / 2.0),
                "2x terrain longitude span");
        require(near(centered_zoom.north - centered_zoom.south, full_latitude_span / 2.0),
                "2x terrain latitude span");

        offgrid::TerrainMapInfo route_overlay;
        std::ostringstream route_output;
        require(offgrid::render_terrain_ansi(
                    *ny_map, 96, 28, true, route_output, route_overlay,
                    terrain_error, offgrid::TerrainViewport{0.5, 0.5, 4.0}),
                "high-contrast route overlay rendering");
        require(route_output.str().find("38;2;255;215;40") != std::string::npos,
                "single-cell gold trail rendering");
        require(route_output.str().find("38;2;235;235;220") != std::string::npos ||
                    route_output.str().find("48;2;235;235;220") != std::string::npos,
                "primary-road rendering");
        require(route_output.str().find("38;2;35;120;230") != std::string::npos ||
                    route_output.str().find("48;2;35;120;230") != std::string::npos,
                "hydrography rendering");
        require(!route_overlay.visible_landmarks.empty(), "visible landmark placement");

        offgrid::MapAnnotations rendered_annotations;
        const auto center_lat = (terrain.north + terrain.south) / 2.0;
        const auto center_lon = (terrain.west + terrain.east) / 2.0;
        rendered_annotations.markers = {
            {"1", center_lat, center_lon - 0.03, "Homestead", "Test Base"},
            {"2", center_lat, center_lon + 0.03, "Water", "Test Water"}};
        offgrid::TerrainMapInfo annotation_info;
        std::ostringstream annotation_output;
        require(offgrid::render_terrain_ansi(
                    *ny_map, 96, 28, true, annotation_output, annotation_info,
                    terrain_error, offgrid::TerrainViewport{0.5, 0.5, 4.0},
                    &rendered_annotations),
                "user field-marker rendering");
        require(annotation_output.str().find("38;2;80;255;90") != std::string::npos,
                "field marker phosphor-green rendering");
        require(annotation_output.str().find("38;2;255;140;20") != std::string::npos ||
                    annotation_output.str().find("48;2;255;140;20") != std::string::npos,
                "waypoint route orange rendering");

        offgrid::TerrainMapInfo east_pan;
        std::ostringstream east_output;
        require(offgrid::render_terrain_ansi(
                    *ny_map, 40, 10, false, east_output, east_pan,
                    terrain_error, offgrid::TerrainViewport{0.6, 0.5, 2.0}),
                "panned terrain rendering");
        require(east_pan.west > centered_zoom.west && east_pan.east > centered_zoom.east,
                "terrain east pan bounds");

        offgrid::TerrainMapInfo florida;
        require(fl_map != maps.end() && offgrid::inspect_terrain(*fl_map, florida, terrain_error),
                "Florida GeoTIFF terrain inspection");
        require(florida.source_width == 1200 && florida.source_height == 1000,
                "Florida preview dimensions");
        require(florida.minimum_elevation_m < -18.0 && florida.maximum_elevation_m > 100.0,
                "Florida preview elevation range");
        require(florida.trail_feature_count == 9490, "Florida trail overlay discovery");
        require(florida.rail_feature_count == 4486, "Florida railroad overlay discovery");
        std::ostringstream florida_output;
        require(offgrid::render_terrain_ansi(
                    *fl_map, 96, 28, true, florida_output, florida,
                    terrain_error, offgrid::TerrainViewport{0.5, 0.5, 1.0}),
                "Florida high-contrast route rendering");
        require(florida_output.str().find("38;2;255;215;40") != std::string::npos,
                "Florida gold trail rendering");
        require(florida_output.str().find("38;2;70;230;255") != std::string::npos,
                "Florida cyan railroad rendering");
    }

    if (offgrid::herb_database_support_available()) {
        const auto database = offgrid::herb_database_path(offgrid::resource_root());
        offgrid::HerbDatabaseStats herbs;
        std::string herb_error;
        require(offgrid::inspect_herb_database(database, herbs, herb_error),
                "plants and herbs database inspection");
        require(herbs.documents == 10 && herbs.pages == 2266,
                "plants and herbs corpus size");
        require(herbs.structured_plants == 0 && herbs.reviewed_statements == 0,
                "unreviewed herbal claims remain disabled");
        const auto poison_hits = offgrid::search_herb_database(
            database, offgrid::resource_root(), "poison hemlock", 5, herb_error);
        require(!poison_hits.empty(), "plants and herbs full-text search");
        if (!poison_hits.empty()) {
            const auto source_page = offgrid::read_herb_page(
                database, offgrid::resource_root(), poison_hits.front().document_id,
                poison_hits.front().page, herb_error);
            require(source_page.has_value(), "plants and herbs source page read");
            require(source_page && source_page->text.find("Hemlock") != std::string::npos,
                    "poison hemlock source-page content");
        }
    }

    std::cout << "All Phase-A core tests passed.\n";
    return 0;
}

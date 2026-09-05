#include "offgrid/energy.hpp"
#include "offgrid/guide.hpp"
#include "offgrid/herbs.hpp"
#include "offgrid/image.hpp"
#include "offgrid/io_scout.hpp"
#include "offgrid/journal.hpp"
#include "offgrid/library.hpp"
#include "offgrid/map.hpp"
#include "offgrid/mesh.hpp"
#include "offgrid/telemetry.hpp"
#include "offgrid/terminal.hpp"
#include "offgrid/ui.hpp"

#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void help() {
    std::cout
        << "WAYKEEPER // OFF-GRID Assistant Phase A\n"
        << "  ask <question>\n"
        << "  ui\n"
        << "  cost <watts> <tokens/sec> <input tokens> <output tokens> <net solar watts>\n"
        << "  map <terrain.tif> [columns] [rows]\n"
        << "  image <manual.pdf> <page> [columns] [rows]\n"
        << "  herbs <search terms>\n"
        << "  parse 'OGP1|source|metric|value|unit|unix_ms|quality'\n"
        << "  scout status|ports|protocols|configure|authorize|tx|probe|listen|baud-scan|manual|send\n"
        << "  mesh status|qualify|on|eco|off|nick|send|wipe\n"
        << "  status\n";
}

std::string join_arguments(const int argc, char** argv, const int first) {
    std::ostringstream joined;
    for (int index = first; index < argc; ++index) {
        if (index > first) joined << ' ';
        joined << argv[index];
    }
    return joined.str();
}

void print_scout_profile(const offgrid::ScoutProfile& profile) {
    std::cout << "Master I/O: " << profile.name << '\n'
              << "Transport: " << offgrid::scout_transport_name(profile.transport) << '\n'
              << "Adapter: " << offgrid::scout_adapter_name(profile.adapter) << '\n'
              << "Endpoint: " << profile.endpoint;
    if (profile.port > 0) std::cout << ':' << profile.port;
    std::cout << '\n' << "Serial: " << profile.baud << " baud / " << profile.data_bits
              << profile.parity.front() << profile.stop_bits << " / " << profile.flow_control << '\n'
              << "Manual query: " << (profile.manual_query.empty() ? "UNASSIGNED" : profile.manual_query) << '\n'
              << "Authorization: " << (profile.authorized ? "RECORDED" : "LOCKED")
              << " / TX " << (profile.allow_transmit ? "ENABLED" : "LOCKED") << '\n';
}

int scout_cli(const int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) == "help") {
        std::cout
            << "WAYKEEPER UART SCOUT // CLI\n"
            << "  scout status\n"
            << "  scout ports\n"
            << "  scout protocols\n"
            << "  scout configure <serial|tcp|telnet|rfc2217|vcom> <endpoint> <baud|port> [adapter] [manual query]\n"
            << "  scout authorize               # records operator authorization; TX remains locked\n"
            << "  scout tx <on|off>             # explicit write gate\n"
            << "  scout probe                   # read-only port open or TCP carrier test\n"
            << "  scout listen [milliseconds]   # serial receive only\n"
            << "  scout baud-scan [milliseconds-per-rate]\n"
            << "  scout manual [query]          # local archive search\n"
            << "  scout ghostline status|start|stop|capture\n"
            << "  scout send <exact payload>    # requires authorization + TX on\n";
        return argc < 3 ? 1 : 0;
    }

    const std::string action = argv[2];
    if (action == "ports") {
        const auto endpoints = offgrid::discover_scout_endpoints();
        std::cout << "LOCAL SERIAL ENDPOINTS: " << endpoints.size() << '\n';
        for (const auto& endpoint : endpoints) {
            std::cout << endpoint.path << "  // " << endpoint.kind << " // " << endpoint.detail << '\n';
        }
        return endpoints.empty() ? 2 : 0;
    }
    if (action == "protocols") {
        std::cout
            << "TRANSPORTS  SERIAL/USB-UART | TCP/ETHERNET | TELNET | RFC2217 | VIRTUAL COM\n"
            << "ADAPTERS    RAW CONSOLE | MODEM/AT | SENSOR/OGP1 | ATA/SIP/VOIP | GHOSTLINE TCP | FLIPPER ZERO\n"
            << "BUSES       I2C/SPI/CAN/RS-485 REQUIRE AN ISOLATED HARDWARE ADAPTER; THEY ARE NOT COM PORT PROTOCOLS\n"
            << "NOTE        COM0COM IS A WINDOWS VIRTUAL-PORT PROVIDER. GHOSTLINE IS TCP-ONLY AND NEEDS A WAYKEEPER PROFILE.\n";
        return 0;
    }
    if (action == "configure") {
        if (argc < 6) {
            std::cerr << "Usage: scout configure <transport> <endpoint> <baud|port> [adapter] [manual query]\n";
            return 2;
        }
        offgrid::ScoutProfile profile;
        if (!offgrid::parse_scout_transport(argv[3], profile.transport)) {
            std::cerr << "Unknown transport. Run 'scout protocols'.\n";
            return 2;
        }
        profile.endpoint = argv[4];
        try {
            if (profile.transport == offgrid::ScoutTransport::Serial ||
                profile.transport == offgrid::ScoutTransport::VirtualCom) profile.baud = std::stoi(argv[5]);
            else profile.port = std::stoi(argv[5]);
        } catch (...) {
            std::cerr << "Baud or TCP port must be numeric.\n";
            return 2;
        }
        int manual_start = 6;
        if (argc >= 7) {
            if (!offgrid::parse_scout_adapter(argv[6], profile.adapter)) {
                std::cerr << "Unknown adapter. Run 'scout protocols'.\n";
                return 2;
            }
            manual_start = 7;
        }
        if (argc > manual_start) profile.manual_query = join_arguments(argc, argv, manual_start);
        std::string error;
        if (!offgrid::save_scout_profile(profile, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        print_scout_profile(profile);
        std::cout << "PROFILE SAVED // PASSIVE PROBE AVAILABLE // TX REMAINS LOCKED\n";
        return 0;
    }

    offgrid::ScoutProfile profile;
    std::string error;
    if (!offgrid::load_scout_profile(profile, error)) {
        std::cerr << error << " Run 'scout configure'.\n";
        return 2;
    }
    if (action == "status") {
        print_scout_profile(profile);
        return 0;
    }
    if (action == "ghostline") {
        const std::string ghostline_action = argc >= 4 ? argv[3] : "status";
        auto runtime = offgrid::ghostline_status(profile);
        if (ghostline_action == "status") {
            std::cout << "GHOSTLINE " << (runtime.installed ? "INSTALLED" : "MISSING")
                      << " / " << (runtime.running ? "RUNNING" : "STOPPED") << '\n'
                      << "Binary: " << (runtime.binary.empty() ? "NOT FOUND" : runtime.binary.string()) << '\n'
                      << "PID: " << runtime.process_id << '\n'
                      << "Local endpoint: " << runtime.local_endpoint << '\n'
                      << "Capture: " << runtime.capture_path << '\n'
                      << "PCAP: " << runtime.pcap_path << '\n'
                      << runtime.detail << '\n';
            return runtime.installed ? 0 : 2;
        }
        if (ghostline_action == "start") {
            if (!offgrid::start_ghostline_observer(profile, runtime, error)) {
                std::cerr << error << '\n';
                return 2;
            }
            std::cout << "GHOSTLINE OBSERVER STARTED // PID " << runtime.process_id
                      << " // CONNECT VIA " << runtime.local_endpoint << '\n';
            return 0;
        }
        if (ghostline_action == "stop") {
            if (!offgrid::stop_ghostline_observer(runtime, error)) {
                std::cerr << error << '\n';
                return 2;
            }
            std::cout << runtime.detail << '\n';
            return 0;
        }
        if (ghostline_action == "capture") {
            std::size_t tail = 24;
            if (argc >= 5) tail = static_cast<std::size_t>(std::stoull(argv[4]));
            std::string rendered;
            if (!offgrid::render_ghostline_capture(
                    runtime.capture_path, tail, rendered, error)) {
                std::cerr << error << '\n';
                return 2;
            }
            std::cout << rendered;
            return 0;
        }
        std::cerr << "Usage: scout ghostline status|start|stop|capture [tail-records]\n";
        return 2;
    }
    if (action == "authorize") {
        profile.authorized = true;
        profile.allow_transmit = false;
        if (!offgrid::save_scout_profile(profile, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "TARGET AUTHORIZATION RECORDED // TX REMAINS LOCKED\n";
        return 0;
    }
    if (action == "tx") {
        if (argc != 4 || (std::string(argv[3]) != "on" && std::string(argv[3]) != "off")) {
            std::cerr << "Usage: scout tx <on|off>\n";
            return 2;
        }
        if (std::string(argv[3]) == "on" && !profile.authorized) {
            std::cerr << "Run 'scout authorize' for a device you own or are authorized to service.\n";
            return 2;
        }
        profile.allow_transmit = std::string(argv[3]) == "on";
        if (!offgrid::save_scout_profile(profile, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "TRANSMIT " << (profile.allow_transmit ? "ENABLED" : "LOCKED") << '\n';
        return 0;
    }
    if (action == "probe") {
        const auto result = offgrid::probe_scout_profile(profile);
        std::cout << result.status << " // " << result.detail << '\n';
        return result.reachable ? 0 : 2;
    }
    if (action == "listen") {
        int milliseconds = 2000;
        if (argc >= 4) milliseconds = std::max(100, std::stoi(argv[3]));
        std::string bytes;
        if (!offgrid::scout_read(profile, milliseconds, bytes, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (bytes.empty()) std::cout << "NO BYTES OBSERVED\n";
        return 0;
    }
    if (action == "baud-scan") {
        int milliseconds = 350;
        if (argc >= 4) milliseconds = std::max(100, std::stoi(argv[3]));
        const auto results = offgrid::passive_baud_scan(profile, milliseconds, error);
        if (results.empty() && !error.empty()) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "PASSIVE BAUD SCORE // OPENING SOME USB-UART DEVICES MAY STILL TOGGLE MODEM LINES\n";
        for (const auto& result : results) {
            std::cout << std::setw(6) << result.baud << "  SCORE " << std::setw(3) << result.score
                      << "  BYTES " << std::setw(5) << result.bytes_observed
                      << "  PRINTABLE " << std::fixed << std::setprecision(0)
                      << result.printable_ratio * 100.0 << "%\n";
        }
        return 0;
    }
    if (action == "manual") {
        auto query = argc >= 4 ? join_arguments(argc, argv, 3) : profile.manual_query;
        if (query.empty()) {
            std::cerr << "No manual query assigned.\n";
            return 2;
        }
        offgrid::SurvivalLibrary library;
        if (!library.load(offgrid::resource_root() / "library" / "catalog.tsv", error)) {
            std::cerr << error << '\n';
            return 2;
        }
        const auto hits = library.search(query, 12);
        for (const auto& hit : hits) {
            const auto& document = library.documents()[hit.document_index];
            std::cout << document.title << " // " << document.category << " // PAGE "
                      << hit.page << '\n' << hit.snippet << "\n\n";
        }
        return hits.empty() ? 2 : 0;
    }
    if (action == "send" && argc >= 4) {
        std::string response;
        if (!offgrid::scout_send(profile, join_arguments(argc, argv, 3), 1200, response, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "TX COMPLETE // RX " << response.size() << " BYTES\n";
        std::cout.write(response.data(), static_cast<std::streamsize>(response.size()));
        if (!response.empty() && response.back() != '\n') std::cout << '\n';
        return 0;
    }
    std::cerr << "Unknown scout command. Run 'scout help'.\n";
    return 2;
}

void print_mesh_status(const offgrid::MeshProfile& profile) {
    const auto readiness = offgrid::inspect_mesh_readiness();
    std::cout << "WAYKEEPER NEARBY MESH // PROTOTYPE\n"
              << "Nickname: " << profile.nickname << '\n'
              << "Channel: " << profile.channel << '\n'
              << "Requested mode: " << offgrid::mesh_power_mode_name(profile.requested_mode) << '\n'
              << "Host: " << (readiness.linux_host ? "LINUX" : "DEVELOPMENT") << '\n'
              << "BlueZ: " << (readiness.bluez_runtime ? "DETECTED" : "NOT DETECTED") << '\n'
              << "Controller: " << readiness.controller << '\n'
              << "Protocol TX: " << (readiness.protocol_backend_ready ? "READY" : "LOCKED") << '\n'
              << "Detail: " << readiness.detail << '\n';
}

int mesh_cli(const int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) == "help") {
        std::cout
            << "WAYKEEPER NEARBY BLE MESH // BUILD MILESTONE\n"
            << "  mesh status                 show profile and radio gate\n"
            << "  mesh qualify                inspect Linux BlueZ/controller readiness\n"
            << "  mesh on|eco|off             save requested power mode\n"
            << "  mesh nick <nickname>        1-24 letters, digits, '-' or '_'\n"
            << "  mesh send <message>         locked until BitChat interoperability gate\n"
            << "  mesh wipe WIPE              delete exact local mesh state files\n"
            << "\nTHIS BUILD DOES NOT YET TRANSMIT OR CLAIM BITCHAT INTEROPERABILITY.\n";
        return argc < 3 ? 1 : 0;
    }

    offgrid::MeshProfile profile;
    std::string error;
    if (!offgrid::load_mesh_profile(profile, error)) {
        std::cerr << error << '\n';
        return 2;
    }
    const std::string action = argv[2];
    if (action == "status" || action == "qualify") {
        print_mesh_status(profile);
        return offgrid::inspect_mesh_readiness().protocol_backend_ready ? 0 : 2;
    }
    offgrid::MeshPowerMode mode;
    if (offgrid::parse_mesh_power_mode(action, mode)) {
        profile.requested_mode = mode;
        if (!offgrid::save_mesh_profile(profile, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "REQUESTED MODE " << offgrid::mesh_power_mode_name(mode)
                  << " SAVED // PROTOCOL TX REMAINS LOCKED\n";
        return 0;
    }
    if (action == "nick" && argc >= 4) {
        profile.nickname = argv[3];
        if (!offgrid::save_mesh_profile(profile, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "MESH NICKNAME SAVED: " << profile.nickname << '\n';
        return 0;
    }
    if (action == "send") {
        if (argc < 4 || join_arguments(argc, argv, 3).size() > offgrid::mesh_message_max_bytes) {
            std::cerr << "Message must contain 1-" << offgrid::mesh_message_max_bytes
                      << " bytes.\n";
            return 2;
        }
        std::cerr << "PROTOCOL TX LOCKED // COMPLETE BLUEZ + BITCHAT INTEROPERABILITY GATE FIRST\n";
        return 2;
    }
    if (action == "wipe") {
        if (argc != 4 || std::string(argv[3]) != "WIPE") {
            std::cerr << "Usage: mesh wipe WIPE\n";
            return 2;
        }
        if (!offgrid::wipe_mesh_state(error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "LOCAL MESH PROFILE, HISTORY, IDENTITY, AND OUTBOX REMOVED\n";
        return 0;
    }
    std::cerr << "Unknown mesh command. Run 'mesh help'.\n";
    return 2;
}

}  // namespace

int main(const int argc, char** argv) {
    const bool ansi = offgrid::enable_ansi_terminal();
    if (argc < 2) {
        return offgrid::run_survival_ui();
    }

    const std::string command = argv[1];
    if (command == "scout") return scout_cli(argc, argv);
    if (command == "mesh" || command == "chat") return mesh_cli(argc, argv);
    if (command == "ask") {
        const auto question = join_arguments(argc, argv, 2);
        if (const auto card = offgrid::find_reviewed_card(question)) {
            if (ansi) std::cout << "\033[1;32m";
            std::cout << card->title;
            if (ansi) std::cout << "\033[0m";
            std::cout << "\n\n" << card->answer
                      << "\n\nSource: " << card->source_url
                      << "\nReviewed: " << card->reviewed_on << '\n';
            return 0;
        }
        std::cout << "No reviewed static card matched. Next route: indexed-manual search; "
                     "only then use the local model.\n";
        return 2;
    }

    if (command == "cost" && argc == 7) {
        const offgrid::InferenceProfile profile{
            "calibrated-device", std::atof(argv[2]), std::atof(argv[3]), 0.0};
        const auto estimate = offgrid::estimate_query_cost(
            profile,
            static_cast<std::size_t>(std::strtoull(argv[4], nullptr, 10)),
            static_cast<std::size_t>(std::strtoull(argv[5], nullptr, 10)),
            std::atof(argv[6]));
        std::cout << std::fixed << std::setprecision(4)
                  << "Runtime: " << estimate.runtime_seconds << " s\n"
                  << "Energy: " << estimate.watt_hours << " Wh\n"
                  << "Energy/token: " << estimate.milliwatt_hours_per_token << " mWh\n"
                  << "Solar payback: " << estimate.solar_recharge_minutes << " min\n";
        return 0;
    }

    if (command == "parse" && argc == 3) {
        const auto reading = offgrid::parse_telemetry(argv[2]);
        if (!reading) {
            std::cerr << "Invalid OGP1 telemetry line.\n";
            return 2;
        }
        std::cout << reading->source << ' ' << reading->metric << '=' << reading->value
                  << ' ' << reading->unit << " [" << offgrid::quality_name(reading->quality)
                  << "] @ " << reading->unix_milliseconds << '\n';
        return 0;
    }

    if (command == "status") {
        offgrid::HerbDatabaseStats herbs;
        std::string herb_error;
        const bool herbs_ready = offgrid::inspect_herb_database(
            offgrid::herb_database_path(offgrid::resource_root()), herbs, herb_error);
        offgrid::JournalStore journal(offgrid::journal_path());
        std::string journal_error;
        const auto active_entries = journal.entries(journal_error).size();
        journal_error.clear();
        const auto archived_entries = journal.archived_entries(journal_error).size();
        std::cout << "Mode: WayKeeper ANSI core / desktop + ARM64 embedded profiles\n"
                  << "Guide: reviewed cards; legacy Ollama adapter compile-disabled by default\n"
                  << "Manual library: imported PDF and text readers with page citations\n"
                  << "Local model: retained in source, disabled for embedded targets\n"
                  << "Plants/herbs: " << (herbs_ready ? std::to_string(herbs.documents) +
                      " PDFs / " + std::to_string(herbs.pages) + " searchable pages" : "not available") << '\n'
                  << "Journal: " << active_entries << " active / " << archived_entries << " archived\n"
                  << "Embedded radios: managed by NetworkManager/BlueZ image services\n";
        offgrid::MeshProfile mesh;
        std::string mesh_error;
        if (offgrid::load_mesh_profile(mesh, mesh_error)) {
            const auto readiness = offgrid::inspect_mesh_readiness();
            std::cout << "Nearby mesh: "
                      << offgrid::mesh_power_mode_name(mesh.requested_mode)
                      << " requested / protocol TX "
                      << (readiness.protocol_backend_ready ? "ready" : "locked") << '\n';
        }
        return 0;
    }

    if (command == "herbs" && argc >= 3) {
        const auto root = offgrid::resource_root();
        std::string error;
        const auto hits = offgrid::search_herb_database(
            offgrid::herb_database_path(root), root, join_arguments(argc, argv, 2), 12, error);
        if (!error.empty()) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "REFERENCE ONLY: a text match is not plant identification or treatment.\n";
        for (const auto& hit : hits) {
            std::cout << "\n" << hit.title << " // PDF PAGE " << hit.page << '\n'
                      << hit.excerpt << '\n';
        }
        return hits.empty() ? 2 : 0;
    }

    if (command == "map" && argc >= 3) {
        std::size_t columns = 96;
        std::size_t rows = 28;
        if (argc >= 4) columns = static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10));
        if (argc >= 5) rows = static_cast<std::size_t>(std::strtoull(argv[4], nullptr, 10));
        offgrid::TerrainMapInfo info;
        std::string error;
        if (!offgrid::render_terrain_ansi(argv[2], columns, rows, ansi, std::cout, info, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << std::fixed << std::setprecision(4)
                  << "Bounds: W " << info.west << " S " << info.south
                  << " E " << info.east << " N " << info.north << '\n'
                  << std::setprecision(0) << "Elevation: " << info.minimum_elevation_m
                  << " to " << info.maximum_elevation_m << " meters\n";
        if (info.trail_feature_count > 0) {
            std::cout << "Trails: " << info.trail_feature_count
                      << " pedestrian segments (amber / *)\n";
        }
        if (info.rail_feature_count > 0) {
            std::cout << "Railroads: " << info.rail_feature_count
                      << " segments (cyan / #; trail crossing X)\n";
        }
        return 0;
    }

    if (command == "image" && argc >= 4) {
        const auto page = static_cast<std::size_t>(std::strtoull(argv[3], nullptr, 10));
        std::size_t columns = 96;
        std::size_t rows = 28;
        if (argc >= 5) columns = static_cast<std::size_t>(std::strtoull(argv[4], nullptr, 10));
        if (argc >= 6) rows = static_cast<std::size_t>(std::strtoull(argv[5], nullptr, 10));
        std::filesystem::path image_path;
        std::string error;
        if (!offgrid::ensure_pdf_page_image(
                argv[2], offgrid::resource_root() / "tmp" / "pdf-page-images",
                std::filesystem::path(argv[2]).stem().string(), page, image_path, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        offgrid::ImageInfo info;
        if (!offgrid::render_image_ansi(
                image_path, columns, rows, ansi, std::cout, info, error)) {
            std::cerr << error << '\n';
            return 2;
        }
        std::cout << "Source: " << info.source_width << 'x' << info.source_height
                  << " / PDF page " << page << '\n';
        return 0;
    }

    if (command == "ui") return offgrid::run_survival_ui();

    help();
    return 1;
}

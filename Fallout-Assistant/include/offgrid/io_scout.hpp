#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace offgrid {

enum class ScoutTransport {
    Serial,
    Tcp,
    Telnet,
    Rfc2217,
    VirtualCom,
};

enum class ScoutAdapter {
    RawConsole,
    ModemAt,
    SensorOgp1,
    SipAta,
    GhostlineTcp,
    FlipperZero,
};

struct ScoutProfile {
    std::string name{"FIELD-I/O-1"};
    ScoutTransport transport{ScoutTransport::Serial};
    ScoutAdapter adapter{ScoutAdapter::RawConsole};
    std::string endpoint;
    int port{};
    int baud{115200};
    int data_bits{8};
    int stop_bits{1};
    std::string parity{"none"};
    std::string flow_control{"none"};
    std::string manual_query;
    bool authorized{};
    bool allow_transmit{};
};

struct ScoutEndpoint {
    std::string path;
    std::string kind;
    std::string detail;
};

struct ScoutProbeResult {
    bool reachable{};
    std::string status;
    std::string detail;
};

struct BaudScanResult {
    int baud{};
    std::size_t bytes_observed{};
    double printable_ratio{};
    int score{};
};

struct GhostlineRuntime {
    bool installed{};
    bool running{};
    long long process_id{-1};
    std::filesystem::path binary;
    std::filesystem::path capture_path;
    std::filesystem::path pcap_path;
    std::filesystem::path log_path;
    std::string local_endpoint;
    std::string detail;
};

std::filesystem::path scout_profile_path();
const std::vector<int>& scout_baud_rates();
std::vector<ScoutEndpoint> discover_scout_endpoints();

std::string scout_transport_name(ScoutTransport transport);
std::string scout_adapter_name(ScoutAdapter adapter);
bool parse_scout_transport(std::string_view value, ScoutTransport& transport);
bool parse_scout_adapter(std::string_view value, ScoutAdapter& adapter);

bool validate_scout_profile(const ScoutProfile& profile, std::string& error);
bool load_scout_profile(ScoutProfile& profile, std::string& error);
bool save_scout_profile(const ScoutProfile& profile, std::string& error);

ScoutProbeResult probe_scout_profile(const ScoutProfile& profile, int timeout_ms = 1200);
std::vector<BaudScanResult> passive_baud_scan(
    const ScoutProfile& profile, int listen_milliseconds, std::string& error);
bool scout_read(
    const ScoutProfile& profile, int listen_milliseconds,
    std::string& bytes, std::string& error);
bool scout_send(
    const ScoutProfile& profile, std::string_view payload,
    int listen_milliseconds, std::string& response, std::string& error);

GhostlineRuntime ghostline_status(const ScoutProfile& profile);
bool start_ghostline_observer(
    const ScoutProfile& profile, GhostlineRuntime& runtime, std::string& error);
bool stop_ghostline_observer(GhostlineRuntime& runtime, std::string& error);
bool render_ghostline_capture(
    const std::filesystem::path& path, std::size_t tail_records,
    std::string& output, std::string& error);

}  // namespace offgrid

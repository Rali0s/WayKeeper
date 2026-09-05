#include "offgrid/io_scout.hpp"

#include "offgrid/library.hpp"
#include "offgrid/profile.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace offgrid {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string upper(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string clean(std::string value, const std::size_t limit = 160) {
    value.erase(std::remove_if(value.begin(), value.end(), [](const unsigned char c) {
        return c == '\r' || c == '\n' || c == '\0';
    }), value.end());
    if (value.size() > limit) value.resize(limit);
    return value;
}

bool serial_transport(const ScoutTransport transport) {
    return transport == ScoutTransport::Serial || transport == ScoutTransport::VirtualCom;
}

bool network_transport(const ScoutTransport transport) {
    return transport == ScoutTransport::Tcp || transport == ScoutTransport::Telnet ||
           transport == ScoutTransport::Rfc2217;
}

#ifndef _WIN32
speed_t baud_constant(const int baud) {
    switch (baud) {
        case 300: return B300;
        case 1200: return B1200;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default: return 0;
    }
}

bool configure_serial_fd(const int fd, const ScoutProfile& profile, std::string& error) {
    termios options{};
    if (tcgetattr(fd, &options) != 0) {
        error = "Could not read serial settings: " + std::string(std::strerror(errno));
        return false;
    }
    const auto speed = baud_constant(profile.baud);
    if (speed == 0) {
        error = "This host does not expose the selected baud rate.";
        return false;
    }
    cfmakeraw(&options);
    options.c_cflag |= CLOCAL | CREAD;
    options.c_cflag &= ~HUPCL;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= profile.data_bits == 7 ? CS7 : CS8;
    options.c_cflag &= ~(PARENB | PARODD);
    if (profile.parity == "even") options.c_cflag |= PARENB;
    else if (profile.parity == "odd") options.c_cflag |= PARENB | PARODD;
    if (profile.stop_bits == 2) options.c_cflag |= CSTOPB;
    else options.c_cflag &= ~CSTOPB;
#ifdef CRTSCTS
    if (profile.flow_control == "hardware") options.c_cflag |= CRTSCTS;
    else options.c_cflag &= ~CRTSCTS;
#endif
    if (profile.flow_control == "xonxoff") options.c_iflag |= IXON | IXOFF;
    else options.c_iflag &= ~(IXON | IXOFF | IXANY);
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        error = "Could not apply serial settings: " + std::string(std::strerror(errno));
        return false;
    }
    return true;
}

bool read_serial_posix(
    const ScoutProfile& profile, const bool transmit, std::string_view payload,
    const int listen_ms, std::string& bytes, std::string& error) {
    const int flags = (transmit ? O_RDWR : O_RDONLY) | O_NOCTTY | O_NONBLOCK;
    const int fd = open(profile.endpoint.c_str(), flags);
    if (fd < 0) {
        error = "Could not open " + profile.endpoint + ": " + std::string(std::strerror(errno));
        return false;
    }
    if (!configure_serial_fd(fd, profile, error)) {
        close(fd);
        return false;
    }
    if (transmit && !payload.empty()) {
        const auto written = write(fd, payload.data(), payload.size());
        if (written < 0 || static_cast<std::size_t>(written) != payload.size()) {
            error = "Serial write failed: " + std::string(std::strerror(errno));
            close(fd);
            return false;
        }
        (void)tcdrain(fd);
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(std::max(0, listen_ms));
    std::array<char, 1024> buffer{};
    while (std::chrono::steady_clock::now() < deadline) {
        const auto count = read(fd, buffer.data(), buffer.size());
        if (count > 0) bytes.append(buffer.data(), static_cast<std::size_t>(count));
        else if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            error = "Serial read failed: " + std::string(std::strerror(errno));
            close(fd);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    close(fd);
    return true;
}
#endif

struct NetworkSocket {
#ifdef _WIN32
    SOCKET value{INVALID_SOCKET};
#else
    int value{-1};
#endif
};

void close_network_socket(NetworkSocket& socket) {
#ifdef _WIN32
    if (socket.value != INVALID_SOCKET) closesocket(socket.value);
    socket.value = INVALID_SOCKET;
#else
    if (socket.value >= 0) close(socket.value);
    socket.value = -1;
#endif
}

bool connect_tcp(
    const ScoutProfile& profile, const int timeout_ms,
    NetworkSocket& connected, std::string& error) {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        error = "Winsock initialization failed.";
        return false;
    }
#endif
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    const auto port = std::to_string(profile.port);
    const int lookup = getaddrinfo(profile.endpoint.c_str(), port.c_str(), &hints, &addresses);
    if (lookup != 0) {
#ifdef _WIN32
        error = "Host lookup failed: " + std::to_string(lookup);
#else
        error = "Host lookup failed: " + std::string(gai_strerror(lookup));
#endif
        return false;
    }
    for (auto* address = addresses; address != nullptr; address = address->ai_next) {
        NetworkSocket candidate;
        candidate.value = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
#ifdef _WIN32
        if (candidate.value == INVALID_SOCKET) continue;
        u_long nonblocking = 1;
        ioctlsocket(candidate.value, FIONBIO, &nonblocking);
        const int result = connect(candidate.value, address->ai_addr,
                                   static_cast<int>(address->ai_addrlen));
        if (result == 0 || WSAGetLastError() == WSAEWOULDBLOCK) {
#else
        if (candidate.value < 0) continue;
        const int old_flags = fcntl(candidate.value, F_GETFL, 0);
        fcntl(candidate.value, F_SETFL, old_flags | O_NONBLOCK);
        const int result = connect(candidate.value, address->ai_addr, address->ai_addrlen);
        if (result == 0 || errno == EINPROGRESS) {
#endif
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(candidate.value, &write_set);
            timeval timeout{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
            const int selected = select(
                static_cast<int>(candidate.value + 1), nullptr, &write_set, nullptr, &timeout);
            int socket_error = 0;
#ifdef _WIN32
            int error_length = sizeof(socket_error);
#else
            socklen_t error_length = sizeof(socket_error);
#endif
            if (selected > 0 && getsockopt(candidate.value, SOL_SOCKET, SO_ERROR,
#ifdef _WIN32
                    reinterpret_cast<char*>(&socket_error),
#else
                    &socket_error,
#endif
                    &error_length) == 0 && socket_error == 0) {
                connected = candidate;
                freeaddrinfo(addresses);
                return true;
            }
        }
        close_network_socket(candidate);
    }
    freeaddrinfo(addresses);
    error = "TCP carrier did not answer before timeout.";
    return false;
}

double printable_ratio(const std::string& bytes) {
    if (bytes.empty()) return 0.0;
    const auto printable = std::count_if(bytes.begin(), bytes.end(), [](const unsigned char value) {
        return std::isprint(value) || value == '\r' || value == '\n' || value == '\t';
    });
    return static_cast<double>(printable) / static_cast<double>(bytes.size());
}

std::filesystem::path ghostline_state_root() {
    return scout_profile_path().parent_path() / "ghostline";
}

std::filesystem::path ghostline_pid_path() {
    return ghostline_state_root() / "ghostline.pid";
}

bool executable_file(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return false;
#ifdef _WIN32
    return true;
#else
    return access(path.c_str(), X_OK) == 0;
#endif
}

std::filesystem::path discover_ghostline_binary() {
    if (const char* configured = std::getenv("OFFGRID_GHOSTLINE_BIN")) {
        if (executable_file(configured)) return configured;
    }
    const auto root = resource_root();
    const std::array<std::filesystem::path, 5> candidates{
        root / "bin" / "ghostline_cli",
        root.parent_path() / "bin" / "ghostline_cli",
        root.parent_path().parent_path() / "github" / "ghostline-gate" /
            "build-waykeeper" / "ghostline_cli",
        std::filesystem::path{"/usr/local/bin/ghostline_cli"},
        std::filesystem::path{"/usr/bin/ghostline_cli"}};
    for (const auto& candidate : candidates) {
        if (executable_file(candidate)) return candidate;
    }
    if (const char* path_value = std::getenv("PATH")) {
#ifdef _WIN32
        constexpr char delimiter = ';';
        constexpr const char* executable = "ghostline_cli.exe";
#else
        constexpr char delimiter = ':';
        constexpr const char* executable = "ghostline_cli";
#endif
        std::istringstream paths(path_value);
        std::string directory;
        while (std::getline(paths, directory, delimiter)) {
            const auto candidate = std::filesystem::path(directory) / executable;
            if (executable_file(candidate)) return candidate;
        }
    }
    return {};
}

long long saved_ghostline_pid() {
    std::ifstream input(ghostline_pid_path());
    long long pid = -1;
    if (!(input >> pid) || pid < 2) return -1;
    return pid;
}

bool process_alive(const long long pid) {
    if (pid < 2) return false;
#ifdef _WIN32
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                       static_cast<DWORD>(pid));
    if (!process) return false;
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
#else
    return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM;
#endif
}

}  // namespace

std::filesystem::path scout_profile_path() {
    return profile_path().parent_path() / "io-scout.ini";
}

const std::vector<int>& scout_baud_rates() {
    static const std::vector<int> rates{
        300, 1200, 2400, 4800, 9600, 19200, 38400, 57600,
        115200, 230400, 460800, 921600};
    return rates;
}

std::string scout_transport_name(const ScoutTransport transport) {
    switch (transport) {
        case ScoutTransport::Serial: return "SERIAL/USB-UART";
        case ScoutTransport::Tcp: return "TCP/ETHERNET";
        case ScoutTransport::Telnet: return "TELNET";
        case ScoutTransport::Rfc2217: return "RFC2217 SERIAL/IP";
        case ScoutTransport::VirtualCom: return "VIRTUAL COM";
    }
    return "UNKNOWN";
}

std::string scout_adapter_name(const ScoutAdapter adapter) {
    switch (adapter) {
        case ScoutAdapter::RawConsole: return "RAW CONSOLE";
        case ScoutAdapter::ModemAt: return "MODEM / AT";
        case ScoutAdapter::SensorOgp1: return "SENSOR / OGP1";
        case ScoutAdapter::SipAta: return "ATA / SIP / VOIP";
        case ScoutAdapter::GhostlineTcp: return "GHOSTLINE TCP";
        case ScoutAdapter::FlipperZero: return "FLIPPER ZERO";
    }
    return "UNKNOWN";
}

bool parse_scout_transport(std::string_view value, ScoutTransport& transport) {
    const auto key = lower(std::string(value));
    if (key == "serial" || key == "uart" || key == "usb" || key == "usb-serial") {
        transport = ScoutTransport::Serial;
    } else if (key == "tcp" || key == "ethernet" || key == "eth") {
        transport = ScoutTransport::Tcp;
    } else if (key == "telnet") transport = ScoutTransport::Telnet;
    else if (key == "rfc2217" || key == "serial-ip" || key == "serialoverethernet") {
        transport = ScoutTransport::Rfc2217;
    } else if (key == "vcom" || key == "virtual-com" || key == "com0com") {
        transport = ScoutTransport::VirtualCom;
    } else return false;
    return true;
}

bool parse_scout_adapter(std::string_view value, ScoutAdapter& adapter) {
    const auto key = lower(std::string(value));
    if (key == "raw" || key == "console" || key == "terminal") adapter = ScoutAdapter::RawConsole;
    else if (key == "modem" || key == "at") adapter = ScoutAdapter::ModemAt;
    else if (key == "sensor" || key == "ogp1") adapter = ScoutAdapter::SensorOgp1;
    else if (key == "ata" || key == "sip" || key == "voip") adapter = ScoutAdapter::SipAta;
    else if (key == "ghostline") adapter = ScoutAdapter::GhostlineTcp;
    else if (key == "flipper" || key == "flipper-zero") adapter = ScoutAdapter::FlipperZero;
    else return false;
    return true;
}

std::vector<ScoutEndpoint> discover_scout_endpoints() {
    std::vector<ScoutEndpoint> endpoints;
#ifdef _WIN32
    std::array<char, 512> target{};
    for (int index = 1; index <= 256; ++index) {
        const auto name = "COM" + std::to_string(index);
        if (QueryDosDeviceA(name.c_str(), target.data(), static_cast<DWORD>(target.size())) != 0) {
            endpoints.push_back({name, "VIRTUAL/PHYSICAL COM", target.data()});
        }
    }
#else
    const std::array<std::filesystem::path, 2> roots{
        std::filesystem::path{"/dev"}, std::filesystem::path{"/dev/serial/by-id"}};
    std::set<std::string> seen;
    for (const auto& root : roots) {
        std::error_code error;
        if (!std::filesystem::is_directory(root, error)) continue;
        for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
            const auto name = entry.path().filename().string();
#ifdef __APPLE__
            const bool match = root == std::filesystem::path{"/dev"} && name.rfind("cu.", 0) == 0;
#else
            const bool match = root != std::filesystem::path{"/dev"} ||
                name.rfind("ttyUSB", 0) == 0 || name.rfind("ttyACM", 0) == 0 ||
                name.rfind("ttyS", 0) == 0 || name.rfind("ttyAMA", 0) == 0;
#endif
            if (!match) continue;
            const auto path = entry.path().string();
            if (!seen.insert(path).second) continue;
            endpoints.push_back({path,
                root == std::filesystem::path{"/dev/serial/by-id"} ? "STABLE SERIAL ID" : "SERIAL",
                name.find("usbmodemflip") != std::string::npos ? "FLIPPER ZERO CLI (230400)" : "LOCAL DEVICE"});
        }
    }
#endif
    std::sort(endpoints.begin(), endpoints.end(), [](const auto& left, const auto& right) {
        return left.path < right.path;
    });
    return endpoints;
}

bool validate_scout_profile(const ScoutProfile& profile, std::string& error) {
    if (profile.name.empty() || profile.endpoint.empty()) {
        error = "Connection name and endpoint are required.";
        return false;
    }
    if (network_transport(profile.transport) && (profile.port < 1 || profile.port > 65535)) {
        error = "Network transports require a TCP port from 1 through 65535.";
        return false;
    }
    if (serial_transport(profile.transport) &&
        std::find(scout_baud_rates().begin(), scout_baud_rates().end(), profile.baud) ==
            scout_baud_rates().end()) {
        error = "Unsupported baud rate.";
        return false;
    }
    if (profile.data_bits != 7 && profile.data_bits != 8) {
        error = "Data bits must be 7 or 8.";
        return false;
    }
    if (profile.stop_bits != 1 && profile.stop_bits != 2) {
        error = "Stop bits must be 1 or 2.";
        return false;
    }
    if (profile.parity != "none" && profile.parity != "even" && profile.parity != "odd") {
        error = "Parity must be none, even, or odd.";
        return false;
    }
    if (profile.flow_control != "none" && profile.flow_control != "hardware" &&
        profile.flow_control != "xonxoff") {
        error = "Flow control must be none, hardware, or xonxoff.";
        return false;
    }
    if (profile.allow_transmit && !profile.authorized) {
        error = "Transmit cannot be enabled until target authorization is recorded.";
        return false;
    }
    return true;
}

bool save_scout_profile(const ScoutProfile& profile, std::string& error) {
    if (!validate_scout_profile(profile, error)) return false;
    std::error_code directory_error;
    std::filesystem::create_directories(scout_profile_path().parent_path(), directory_error);
    if (directory_error) {
        error = "Could not create I/O state directory: " + directory_error.message();
        return false;
    }
    std::ofstream output(scout_profile_path(), std::ios::trunc);
    if (!output) {
        error = "Could not write I/O profile.";
        return false;
    }
    output << "name=" << clean(profile.name) << '\n'
           << "transport=" << scout_transport_name(profile.transport) << '\n'
           << "adapter=" << scout_adapter_name(profile.adapter) << '\n'
           << "endpoint=" << clean(profile.endpoint) << '\n'
           << "port=" << profile.port << '\n'
           << "baud=" << profile.baud << '\n'
           << "data_bits=" << profile.data_bits << '\n'
           << "stop_bits=" << profile.stop_bits << '\n'
           << "parity=" << profile.parity << '\n'
           << "flow_control=" << profile.flow_control << '\n'
           << "manual_query=" << clean(profile.manual_query) << '\n'
           << "authorized=" << (profile.authorized ? 1 : 0) << '\n'
           << "allow_transmit=" << (profile.allow_transmit ? 1 : 0) << '\n';
    return static_cast<bool>(output);
}

bool load_scout_profile(ScoutProfile& profile, std::string& error) {
    std::ifstream input(scout_profile_path());
    if (!input) {
        error = "No master I/O connection has been configured.";
        return false;
    }
    ScoutProfile loaded;
    std::string line;
    while (std::getline(input, line)) {
        const auto delimiter = line.find('=');
        if (delimiter == std::string::npos) continue;
        const auto key = line.substr(0, delimiter);
        const auto value = clean(line.substr(delimiter + 1));
        try {
            if (key == "name") loaded.name = value;
            else if (key == "transport") {
                if (!parse_scout_transport(value, loaded.transport)) {
                    const auto lower_value = lower(value);
                    if (lower_value.find("serial/usb") != std::string::npos) loaded.transport = ScoutTransport::Serial;
                    else if (lower_value.find("tcp/ethernet") != std::string::npos) loaded.transport = ScoutTransport::Tcp;
                    else if (lower_value.find("rfc2217") != std::string::npos) loaded.transport = ScoutTransport::Rfc2217;
                    else if (lower_value.find("virtual") != std::string::npos) loaded.transport = ScoutTransport::VirtualCom;
                }
            } else if (key == "adapter") {
                if (!parse_scout_adapter(value, loaded.adapter)) {
                    const auto lower_value = lower(value);
                    if (lower_value.find("modem") != std::string::npos) loaded.adapter = ScoutAdapter::ModemAt;
                    else if (lower_value.find("sensor") != std::string::npos) loaded.adapter = ScoutAdapter::SensorOgp1;
                    else if (lower_value.find("sip") != std::string::npos) loaded.adapter = ScoutAdapter::SipAta;
                    else if (lower_value.find("ghostline") != std::string::npos) loaded.adapter = ScoutAdapter::GhostlineTcp;
                    else if (lower_value.find("flipper") != std::string::npos) loaded.adapter = ScoutAdapter::FlipperZero;
                }
            } else if (key == "endpoint") loaded.endpoint = value;
            else if (key == "port") loaded.port = std::stoi(value);
            else if (key == "baud") loaded.baud = std::stoi(value);
            else if (key == "data_bits") loaded.data_bits = std::stoi(value);
            else if (key == "stop_bits") loaded.stop_bits = std::stoi(value);
            else if (key == "parity") loaded.parity = lower(value);
            else if (key == "flow_control") loaded.flow_control = lower(value);
            else if (key == "manual_query") loaded.manual_query = value;
            else if (key == "authorized") loaded.authorized = value == "1";
            else if (key == "allow_transmit") loaded.allow_transmit = value == "1";
        } catch (...) {
            error = "Invalid I/O profile line: " + line;
            return false;
        }
    }
    if (!validate_scout_profile(loaded, error)) return false;
    profile = std::move(loaded);
    return true;
}

ScoutProbeResult probe_scout_profile(const ScoutProfile& profile, const int timeout_ms) {
    std::string validation_error;
    if (!validate_scout_profile(profile, validation_error)) {
        return {false, "INVALID", validation_error};
    }
    if (network_transport(profile.transport)) {
        NetworkSocket socket;
        std::string error;
        if (!connect_tcp(profile, timeout_ms, socket, error)) return {false, "NO CARRIER", error};
        close_network_socket(socket);
        return {true, "CARRIER READY",
            "TCP accepted at " + profile.endpoint + ":" + std::to_string(profile.port) +
            "; service negotiation was not performed."};
    }
#ifdef _WIN32
    const auto device = "\\\\.\\" + profile.endpoint;
    const HANDLE handle = CreateFileA(device.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return {false, "PORT UNAVAILABLE", "COM port could not be opened read-only."};
    CloseHandle(handle);
    return {true, "PORT READY", "Serial endpoint opened read-only; no bytes transmitted."};
#else
    const int fd = open(profile.endpoint.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) return {false, "PORT UNAVAILABLE", std::strerror(errno)};
    close(fd);
    return {true, "PORT READY", "Serial endpoint opened read-only; no bytes transmitted."};
#endif
}

bool scout_read(
    const ScoutProfile& profile, const int listen_milliseconds,
    std::string& bytes, std::string& error) {
    bytes.clear();
    if (!serial_transport(profile.transport)) {
        error = "Passive read currently supports local serial and virtual COM profiles only.";
        return false;
    }
#ifdef _WIN32
    error = "Windows passive serial read is reserved for the libserialport adapter.";
    return false;
#else
    return read_serial_posix(profile, false, {}, listen_milliseconds, bytes, error);
#endif
}

std::vector<BaudScanResult> passive_baud_scan(
    const ScoutProfile& profile, const int listen_milliseconds, std::string& error) {
    std::vector<BaudScanResult> results;
    if (!serial_transport(profile.transport)) {
        error = "Baud scanning applies only to serial transports.";
        return results;
    }
    for (const int baud : scout_baud_rates()) {
        ScoutProfile candidate = profile;
        candidate.baud = baud;
        std::string bytes;
        std::string read_error;
        if (!scout_read(candidate, listen_milliseconds, bytes, read_error)) {
            if (results.empty()) error = read_error;
            continue;
        }
        const double ratio = printable_ratio(bytes);
        int score = static_cast<int>(ratio * 70.0);
        if (bytes.find('\n') != std::string::npos || bytes.find('\r') != std::string::npos) score += 15;
        if (bytes.find("OGP1|") != std::string::npos) score += 15;
        if (bytes.empty()) score = 0;
        results.push_back({baud, bytes.size(), ratio, std::min(score, 100)});
    }
    std::stable_sort(results.begin(), results.end(), [](const auto& left, const auto& right) {
        return left.score > right.score;
    });
    return results;
}

bool scout_send(
    const ScoutProfile& profile, const std::string_view payload,
    const int listen_milliseconds, std::string& response, std::string& error) {
    response.clear();
    if (!profile.authorized || !profile.allow_transmit) {
        error = "Transmit is locked. Record target authorization and explicitly enable TX.";
        return false;
    }
    if (payload.empty() || payload.size() > 4096) {
        error = "Payload must contain 1 through 4096 bytes.";
        return false;
    }
    if (serial_transport(profile.transport)) {
#ifdef _WIN32
        error = "Windows serial transmit is reserved for the libserialport adapter.";
        return false;
#else
        return read_serial_posix(profile, true, payload, listen_milliseconds, response, error);
#endif
    }
    NetworkSocket socket;
    if (!connect_tcp(profile, 1500, socket, error)) return false;
#ifdef _WIN32
    const int sent = send(socket.value, payload.data(), static_cast<int>(payload.size()), 0);
#else
    const auto sent = send(socket.value, payload.data(), payload.size(), 0);
#endif
    if (sent < 0 || static_cast<std::size_t>(sent) != payload.size()) {
        error = "TCP send failed.";
        close_network_socket(socket);
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(std::max(0, listen_milliseconds));
    std::array<char, 1024> buffer{};
    while (std::chrono::steady_clock::now() < deadline) {
        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(socket.value, &read_set);
        timeval timeout{0, 50000};
        if (select(static_cast<int>(socket.value + 1), &read_set, nullptr, nullptr, &timeout) > 0) {
#ifdef _WIN32
            const int count = recv(socket.value, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
            const auto count = recv(socket.value, buffer.data(), buffer.size(), 0);
#endif
            if (count > 0) response.append(buffer.data(), static_cast<std::size_t>(count));
            else break;
        }
    }
    close_network_socket(socket);
    return true;
}

GhostlineRuntime ghostline_status(const ScoutProfile& profile) {
    GhostlineRuntime runtime;
    runtime.binary = discover_ghostline_binary();
    runtime.installed = !runtime.binary.empty();
    runtime.capture_path = ghostline_state_root() / "field-io.glcap";
    runtime.pcap_path = ghostline_state_root() / "field-io.pcap";
    runtime.log_path = ghostline_state_root() / "ghostline.log";
    runtime.local_endpoint = "127.0.0.1:17777";
    runtime.process_id = saved_ghostline_pid();
    runtime.running = process_alive(runtime.process_id);
    if (!runtime.installed) runtime.detail = "Ghostline executable not found.";
    else if (runtime.running) runtime.detail = "Observe-only capture process is running.";
    else if (runtime.process_id > 1) runtime.detail = "Ghostline PID record is stale.";
    else runtime.detail = "Ghostline is installed and stopped.";
    if (profile.transport == ScoutTransport::Telnet ||
        profile.transport == ScoutTransport::Rfc2217) {
        runtime.detail += " Capture is raw TCP; session negotiation remains with the client.";
    }
    return runtime;
}

bool start_ghostline_observer(
    const ScoutProfile& profile, GhostlineRuntime& runtime, std::string& error) {
    runtime = ghostline_status(profile);
    if (!runtime.installed) {
        error = runtime.detail;
        return false;
    }
    if (runtime.running) {
        error = "Ghostline is already running.";
        return false;
    }
    if (!profile.authorized) {
        error = "Record authorization before capturing device traffic.";
        return false;
    }
    if (profile.adapter != ScoutAdapter::GhostlineTcp) {
        error = "Select the GHOSTLINE adapter in the master I/O profile first.";
        return false;
    }
    if (!validate_scout_profile(profile, error)) return false;
#ifdef _WIN32
    error = "WayKeeper Ghostline process launch is not yet available on Windows.";
    return false;
#else
    std::error_code directory_error;
    std::filesystem::create_directories(ghostline_state_root(), directory_error);
    if (directory_error) {
        error = "Could not create Ghostline state directory: " + directory_error.message();
        return false;
    }
    std::vector<std::string> arguments;
    arguments.push_back(runtime.binary.string());
    const bool serial = serial_transport(profile.transport);
    if (serial) {
        arguments.insert(arguments.end(), {
            "--serial-device", profile.endpoint,
            "--baud", std::to_string(profile.baud),
            "--listen-host", "127.0.0.1",
            "--listen-port", "17777"});
    } else {
        arguments.insert(arguments.end(), {
            "17777", profile.endpoint, std::to_string(profile.port)});
    }
    arguments.insert(arguments.end(), {
        "--observe-only",
        "--capture", runtime.capture_path.string(),
        "--capture-pcap", runtime.pcap_path.string(),
        "--capture-max-bytes", "4194304",
        "--capture-snaplen", "1024",
        "--audit-log", (ghostline_state_root() / "audit.log").string(),
        "--audit-json", (ghostline_state_root() / "audit.jsonl").string(),
        "--action-log", (ghostline_state_root() / "actions.log").string(),
        "--review-queue-dir", (ghostline_state_root() / "review").string()});
    if (profile.port == 1883) {
        arguments.insert(arguments.end(), {"--protocol-hint", "mqtt", "--expect-connack"});
    } else if (serial) {
        arguments.insert(arguments.end(), {"--protocol-hint", "raw-live"});
    }

    const pid_t pid = fork();
    if (pid < 0) {
        error = "Could not fork Ghostline: " + std::string(std::strerror(errno));
        return false;
    }
    if (pid == 0) {
        const int log_fd = open(runtime.log_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0640);
        if (log_fd >= 0) {
            dup2(log_fd, STDOUT_FILENO);
            dup2(log_fd, STDERR_FILENO);
            close(log_fd);
        }
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (auto& argument : arguments) argv.push_back(argument.data());
        argv.push_back(nullptr);
        execv(runtime.binary.c_str(), argv.data());
        _exit(127);
    }
    {
        std::ofstream pid_file(ghostline_pid_path(), std::ios::trunc);
        if (!pid_file) {
            kill(pid, SIGTERM);
            error = "Could not persist Ghostline PID.";
            return false;
        }
        pid_file << pid << '\n';
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    int child_status = 0;
    if (waitpid(pid, &child_status, WNOHANG) == pid) {
        std::filesystem::remove(ghostline_pid_path(), directory_error);
        error = "Ghostline exited during startup; inspect " + runtime.log_path.string();
        runtime = ghostline_status(profile);
        return false;
    }
    runtime = ghostline_status(profile);
    if (!runtime.running) {
        error = "Ghostline did not remain active.";
        return false;
    }
    return true;
#endif
}

bool stop_ghostline_observer(GhostlineRuntime& runtime, std::string& error) {
    const auto pid = saved_ghostline_pid();
    if (pid < 2) {
        error = "No Ghostline PID is recorded.";
        return false;
    }
#ifdef _WIN32
    error = "WayKeeper Ghostline process stop is not yet available on Windows.";
    return false;
#else
    if (kill(static_cast<pid_t>(pid), SIGTERM) != 0 && errno != ESRCH) {
        error = "Could not stop Ghostline: " + std::string(std::strerror(errno));
        return false;
    }
    std::error_code remove_error;
    std::filesystem::remove(ghostline_pid_path(), remove_error);
    runtime.process_id = -1;
    runtime.running = false;
    runtime.detail = "Ghostline stop requested; capture artifacts preserved.";
    return true;
#endif
}

bool render_ghostline_capture(
    const std::filesystem::path& path, const std::size_t tail_records,
    std::string& output, std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Could not open Ghostline capture: " + path.string();
        return false;
    }
    std::vector<std::vector<std::string>> records;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("GLCAP1\t", 0) != 0) continue;
        std::vector<std::string> fields;
        std::size_t offset = 0;
        while (offset <= line.size()) {
            const auto delimiter = line.find('\t', offset);
            if (delimiter == std::string::npos) {
                fields.push_back(line.substr(offset));
                break;
            }
            fields.push_back(line.substr(offset, delimiter - offset));
            offset = delimiter + 1;
        }
        if (fields.size() >= 9) records.push_back(std::move(fields));
    }
    if (records.empty()) {
        error = "Capture has no GLCAP1 records.";
        return false;
    }
    const auto first = tail_records > 0 && records.size() > tail_records
        ? records.size() - tail_records : 0;
    std::ostringstream rendered;
    for (std::size_t record_index = first; record_index < records.size(); ++record_index) {
        const auto& fields = records[record_index];
        rendered << fields[1] << "  FLOW " << fields[2] << "  " << upper(fields[3])
                 << "  " << upper(fields[4]) << "  " << fields[5] << " BYTES\n"
                 << fields[6] << '\n';
        const auto& hex = fields[7];
        for (std::size_t byte_offset = 0; byte_offset * 2 < hex.size(); byte_offset += 16) {
            rendered << std::hex << std::setfill('0') << std::setw(6) << byte_offset << "  ";
            std::string ascii;
            for (std::size_t column = 0; column < 16; ++column) {
                const auto index = (byte_offset + column) * 2;
                if (index + 1 >= hex.size()) {
                    rendered << "   ";
                    continue;
                }
                const auto pair = hex.substr(index, 2);
                rendered << pair << ' ';
                try {
                    const auto value = static_cast<unsigned char>(std::stoul(pair, nullptr, 16));
                    ascii.push_back(std::isprint(value) ? static_cast<char>(value) : '.');
                } catch (...) {
                    ascii.push_back('?');
                }
            }
            rendered << " |" << ascii << "|\n";
        }
        rendered << '\n';
    }
    output = rendered.str();
    return true;
}

}  // namespace offgrid

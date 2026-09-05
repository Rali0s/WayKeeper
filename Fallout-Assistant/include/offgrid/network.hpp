#pragma once

#include <string>

namespace offgrid {

struct NetworkStatus {
    bool online{};
    std::string interface_name;
    std::string address;
};

NetworkStatus inspect_network();

}  // namespace offgrid

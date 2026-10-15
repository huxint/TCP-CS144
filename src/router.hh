#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router {
private:
    struct routing_item {
        uint32_t mask_;
        uint32_t route_prefix_;
        std::optional<Address> next_hop_;
        size_t interface_num_;
        routing_item(const uint32_t route_prefix,
                     const uint8_t prefix_length,
                     const std::optional<Address> next_hop,
                     const size_t interface_num)
        : mask_(prefix_length == 0 ? 0 : ~uint32_t{0} << (32 - prefix_length)),
          route_prefix_(route_prefix & mask_),
          next_hop_(next_hop),
          interface_num_(interface_num) {}
    };

public:
    // Add an interface to the router
    // \param[in] interface an already-constructed network interface
    // \returns The index of the interface after it has been added to the router
    size_t add_interface(std::shared_ptr<NetworkInterface> interface) {
        _interfaces.push_back(notnull("add_interface", std::move(interface)));
        return _interfaces.size() - 1;
    }

    // Access an interface by index
    std::shared_ptr<NetworkInterface> interface(const size_t N) {
        return _interfaces.at(N);
    }

    // Add a route (a forwarding rule)
    void add_route(uint32_t route_prefix, uint8_t prefix_length, std::optional<Address> next_hop, size_t interface_num);

    // Route packets between the interfaces
    void route();

private:
    // The router's collection of network interfaces
    std::vector<std::shared_ptr<NetworkInterface>> _interfaces{};
    std::vector<routing_item> routing_table_{};
};

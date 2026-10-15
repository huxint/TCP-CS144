#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/"
         << static_cast<int>(prefix_length) << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)")
         << " on interface " << interface_num << "\n";

    routing_table_.emplace_back(route_prefix, prefix_length, next_hop, interface_num);
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route() {
    for (auto &interface : _interfaces) {
        auto &data_queue = interface->datagrams_received();
        while (!data_queue.empty()) {
            InternetDatagram &data = data_queue.front();
            if (data.header.ttl == 0 || data.header.ttl == 1) {
                data_queue.pop();
                continue;
            }
            uint32_t ip = data.header.dst;
            const routing_item *best_match = nullptr;
            for (const auto &item : routing_table_) {
                if (item.route_prefix_ == (ip & item.mask_)) {
                    if (!best_match || best_match->mask_ < item.mask_) {
                        best_match = &item;
                    }
                }
            }
            if (best_match) {
                data.header.ttl -= 1;
                data.header.compute_checksum();
                auto &next_interface = _interfaces.at(best_match->interface_num_);
                if (best_match->next_hop_.has_value()) {
                    next_interface->send_datagram(data, best_match->next_hop_.value());
                } else {
                    next_interface->send_datagram(data, Address::from_ipv4_numeric(data.header.dst));
                }
            }
            data_queue.pop();
        }
    }
}

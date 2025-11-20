#pragma once

#include <optional>
#include <queue>
#include <unordered_map>

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

/**

ARP = 已知 IP，打听 MAC 的协议。

NetworkInterface = 负责记住这张 IP→MAC 表，并在不知道 MAC 的时候发 ARP 请求打听、缓存数据报，知道之后再发出去。

30 秒过期 = 强制你别太迷信旧映射，网络是会变的。

*/

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface {
public:
    // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
    class OutputPort {
    public:
        virtual void transmit(const NetworkInterface &sender, const EthernetFrame &frame) = 0;
        virtual ~OutputPort() = default;
    };

    // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
    // addresses
    NetworkInterface(std::string_view name,
                     std::shared_ptr<OutputPort> port,
                     const EthernetAddress &ethernet_address,
                     const Address &ip_address);

    // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
    // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
    // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
    void send_datagram(const InternetDatagram &dgram, const Address &next_hop);

    // Receives an Ethernet frame and responds appropriately.
    // If type is IPv4, pushes the datagram to the datagrams_in queue.
    // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    // If type is ARP reply, learn a mapping from the "sender" fields.
    void recv_frame(const EthernetFrame &frame);

    // Called periodically when time elapses
    void tick(size_t ms_since_last_tick);

    // Accessors
    const std::string &name() const {
        return name_;
    }
    const OutputPort &output() const {
        return *port_;
    }
    OutputPort &output() {
        return *port_;
    }
    std::queue<InternetDatagram> &datagrams_received() {
        return datagrams_received_;
    }

private:
    // Human-readable name of the interface
    std::string name_;

    // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
    std::shared_ptr<OutputPort> port_;
    void transmit(const EthernetFrame &frame) const {
        port_->transmit(*this, frame);
    }

    // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
    EthernetAddress ethernet_address_;

    // IP (known as internet-layer or network-layer) address of the interface
    Address ip_address_;

    // Datagrams that have been received
    std::queue<InternetDatagram> datagrams_received_{};

    // 当前时间
    size_t current_time_{};

    // 保存arp表
    /**
    key: ip 地址
    value: first 是mac地址，second 是这个映射的过期时间戳=current_time_ + 30000
    */
    std::unordered_map<uint32_t, std::pair<EthernetAddress, size_t>> arp_table_{};

    // 等待arp请求的信号量队列
    /**
    key: 目标ip
    value:  first 是 一个队列，存准备发但还没 MAC 的以太网帧，second
    是一个可选的时间戳，表示在这个时间点之前不要再给这个 IP 发 ARP 请求
    */
    std::unordered_map<uint32_t, std::pair<std::queue<EthernetFrame>, std::optional<size_t>>> frame_queue_{};
    void make_arp_response(const ARPMessage &message, EthernetFrame &response) const;
    void send_arp_request(const uint32_t target_ip, EthernetFrame &arp_request_frame);
};

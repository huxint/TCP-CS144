#include "network_interface.hh"
#include "exception.hh"

#include <iostream>
#include <utility>

using namespace std;

NetworkInterface::NetworkInterface(string_view name,
                                   shared_ptr<OutputPort> port,
                                   const EthernetAddress &ethernet_address,
                                   const Address &ip_address)
: name_(name),
  port_(notnull("OutputPort", move(port))),
  ethernet_address_(ethernet_address),
  ip_address_(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    EthernetFrame frame;
    const uint32_t target_ip = next_hop.ipv4_numeric();
    frame.header.src = ethernet_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.payload = serialize(dgram);

    const auto mapping = arp_table_.find(target_ip);
    if (mapping != arp_table_.end()) {
        frame.header.dst = mapping->second.ethernet_address;
        transmit(frame);
        return;
    }

    auto &pending = frame_queue_[target_ip];
    pending.frames.push(std::move(frame));
    if (pending.expires_at <= current_time_) {
        // Set the deadline before transmit: an OutputPort may reply synchronously.
        pending.expires_at = current_time_ + 5000;
        send_arp_request(target_ip);
    }
}

void NetworkInterface::send_arp_request(uint32_t target_ip) {
    ARPMessage request;
    request.opcode = ARPMessage::OPCODE_REQUEST;
    request.sender_ethernet_address = ethernet_address_;
    request.sender_ip_address = ip_address_.ipv4_numeric();
    request.target_ip_address = target_ip;

    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.header.dst = ETHERNET_BROADCAST;
    frame.header.src = ethernet_address_;
    frame.payload = serialize(request);
    transmit(frame);
}

void NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
        return;
    }
    if (frame.header.type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (parse(datagram, frame.payload)) {
            datagrams_received_.push(std::move(datagram));
        }
        return;
    }
    if (frame.header.type != EthernetHeader::TYPE_ARP) {
        return;
    }

    ARPMessage message;
    if (!parse(message, frame.payload)) {
        return;
    }
    arp_table_.insert_or_assign(message.sender_ip_address,
                                ARPEntry{message.sender_ethernet_address, current_time_ + 30000});
    if (message.opcode == ARPMessage::OPCODE_REQUEST && message.target_ip_address == ip_address_.ipv4_numeric()) {
        EthernetFrame response;
        make_arp_response(message, response);
        transmit(response);
    }

    const auto pending = frame_queue_.find(message.sender_ip_address);
    if (pending != frame_queue_.end()) {
        auto frames = std::move(pending->second.frames);
        frame_queue_.erase(pending);
        while (!frames.empty()) {
            frames.front().header.dst = message.sender_ethernet_address;
            transmit(frames.front());
            frames.pop();
        }
    }
}

void NetworkInterface::make_arp_response(const ARPMessage &message, EthernetFrame &response) const {
    response.header.dst = message.sender_ethernet_address;
    response.header.src = ethernet_address_;
    response.header.type = EthernetHeader::TYPE_ARP;

    ARPMessage reply;
    reply.opcode = ARPMessage::OPCODE_REPLY;
    reply.sender_ethernet_address = ethernet_address_;
    reply.sender_ip_address = ip_address_.ipv4_numeric();
    reply.target_ethernet_address = message.sender_ethernet_address;
    reply.target_ip_address = message.sender_ip_address;
    response.payload = serialize(reply);
}

void NetworkInterface::tick(size_t ms_since_last_tick) {
    current_time_ += ms_since_last_tick;
    erase_if(arp_table_, [&](const auto &entry) { return entry.second.expires_at <= current_time_; });
    erase_if(frame_queue_, [&](const auto &entry) { return entry.second.expires_at <= current_time_; });
}

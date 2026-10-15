#include "network_interface.hh"
#include "router.hh"
#include "tcp_receiver.hh"
#include "tcp_sender.hh"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

using namespace std;

void require(bool condition, string_view message) {
    if (!condition) {
        throw runtime_error(string(message));
    }
}

void reassembler_eof() {
    Reassembler reassembler{ByteStream{16}};
    reassembler.insert(1, "bc", true);
    reassembler.insert(0, "abc", false);
    string output;
    read(reassembler.reader(), 3, output);
    require(output == "abc" && reassembler.reader().is_finished(), "covering a fragment lost its EOF");

    Reassembler duplicate{ByteStream{16}};
    duplicate.insert(1, "bc", false);
    duplicate.insert(2, "c", true);
    duplicate.insert(0, "a", false);
    read(duplicate.reader(), 3, output);
    require(output == "abc" && duplicate.reader().is_finished(), "duplicate data lost its EOF");
    require(duplicate.bytes_pending() == 0, "duplicate data changed pending bytes");
}

void sender_window_shrink() {
    const Wrap32 isn{137};
    TCPSender sender{ByteStream{64}, isn, 10};
    vector<TCPSenderMessage> messages;
    const auto transmit = [&](const TCPSenderMessage &message) { messages.push_back(message); };
    sender.push(transmit);
    sender.receive({isn + 1, 8});
    messages.clear();
    sender.writer().push("abcdefghijkl");
    sender.push(transmit);
    require(messages.size() == 1 && messages.back().payload == "abcdefgh", "initial window was not respected");

    messages.clear();
    sender.receive({isn + 1, 2});
    sender.push(transmit);
    require(messages.empty() && sender.sequence_numbers_in_flight() == 8, "shrinking window underflowed");
    sender.receive({isn + 1, 0});
    sender.push(transmit);
    require(messages.empty(), "zero window sent another byte while data was outstanding");

    sender.receive({isn + 9, 4});
    sender.push(transmit);
    require(messages.size() == 1 && messages.back().payload == "ijkl", "sending did not resume after ACK");
}

void sender_idle_timer() {
    const Wrap32 isn{137};
    TCPSender sender{ByteStream{64}, isn, 10};
    vector<TCPSenderMessage> messages;
    const auto transmit = [&](const TCPSenderMessage &message) { messages.push_back(message); };
    sender.push(transmit);
    sender.receive({isn + 1, 8});
    messages.clear();
    sender.tick(1000, transmit);
    require(messages.empty(), "acknowledged SYN was retransmitted");

    sender.writer().push("a");
    sender.push(transmit);
    messages.clear();
    sender.tick(9, transmit);
    require(messages.empty(), "idle time was charged to the next transmission");
    sender.tick(1, transmit);
    require(messages.size() == 1 && messages.back().payload == "a", "new timer did not expire at its RTO");
}

void sender_zero_window() {
    const Wrap32 isn{137};
    TCPSender sender{ByteStream{64}, isn, 10};
    vector<TCPSenderMessage> messages;
    const auto transmit = [&](const TCPSenderMessage &message) { messages.push_back(message); };
    sender.push(transmit);
    sender.receive({isn + 1, 0});
    sender.writer().push("a");
    sender.push(transmit);
    messages.clear();
    for (size_t i = 0; i < 12; ++i) {
        sender.tick(10, transmit);
    }
    require(messages.size() == 12, "zero-window probes backed off");
    require(sender.consecutive_retransmissions() == 0, "zero-window probes counted as failed retransmissions");
    sender.receive({isn + 1, 8});
    sender.tick(10, transmit);
    require(sender.consecutive_retransmissions() == 1, "normal retransmission did not resume");
}

void receiver_initial_sequence() {
    const Wrap32 isn{137};
    TCPReceiver receiver{Reassembler{ByteStream{16}}};
    receiver.receive({.seqno = isn, .SYN = true});
    receiver.receive({.seqno = isn + 5, .SYN = true, .payload = "bad"});
    require(receiver.send().ackno == isn + 1 && receiver.reassembler().bytes_pending() == 0,
            "another SYN changed the established stream");

    receiver.receive({.seqno = isn, .payload = "xabc", .FIN = true});
    string output;
    read(receiver.reader(), 3, output);
    require(output == "abc" && receiver.reader().is_finished(), "payload overlapping SYN was discarded");
    require(receiver.send().ackno == isn + 5, "ACK did not include the payload and FIN");
}

class RecordingPort : public NetworkInterface::OutputPort {
public:
    vector<EthernetFrame> frames{};
    void transmit(const NetworkInterface &, const EthernetFrame &frame) override {
        frames.push_back(frame);
    }
};

constexpr EthernetAddress local_mac{2, 0, 0, 0, 0, 1};
constexpr EthernetAddress remote_mac{2, 0, 0, 0, 0, 2};

InternetDatagram datagram(uint32_t destination) {
    InternetDatagram result;
    result.header.src = 0x0a000001;
    result.header.dst = destination;
    result.header.ttl = 64;
    result.payload.emplace_back("payload");
    result.header.len = result.header.hlen * 4 + result.payload.front().size();
    result.header.compute_checksum();
    return result;
}

EthernetFrame arp(uint16_t opcode, uint32_t sender, uint32_t target) {
    ARPMessage message;
    message.opcode = opcode;
    message.sender_ethernet_address = remote_mac;
    message.sender_ip_address = sender;
    message.target_ethernet_address = local_mac;
    message.target_ip_address = target;
    EthernetFrame frame;
    frame.header.src = remote_mac;
    frame.header.dst = opcode == ARPMessage::OPCODE_REQUEST ? ETHERNET_BROADCAST : local_mac;
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.payload = serialize(message);
    return frame;
}

void arp_request_learning() {
    auto port = make_shared<RecordingPort>();
    NetworkInterface interface{"test", port, local_mac, Address{"10.0.0.1"}};
    interface.recv_frame(arp(ARPMessage::OPCODE_REQUEST, 0x0a000002, 0x0a000003));
    require(port->frames.empty(), "replied to an ARP request for a different host");
    interface.send_datagram(datagram(0x0a000002), Address{"10.0.0.2"});
    require(port->frames.size() == 1 && port->frames.back().header.type == EthernetHeader::TYPE_IPv4,
            "did not learn the sender of a broadcast ARP request");

    port->frames.clear();
    interface.send_datagram(datagram(0x0a000003), Address{"10.0.0.3"});
    require(port->frames.size() == 1 && port->frames.back().header.type == EthernetHeader::TYPE_ARP,
            "missing initial ARP request");
    port->frames.clear();
    interface.recv_frame(arp(ARPMessage::OPCODE_REQUEST, 0x0a000003, 0x0a000001));
    require(port->frames.size() == 2 && port->frames[0].header.type == EthernetHeader::TYPE_ARP &&
                port->frames[1].header.type == EthernetHeader::TYPE_IPv4 && port->frames[1].header.dst == remote_mac,
            "ARP request did not release queued data after the reply");
}

void arp_expiration() {
    auto port = make_shared<RecordingPort>();
    NetworkInterface interface{"test", port, local_mac, Address{"10.0.0.1"}};
    interface.recv_frame(arp(ARPMessage::OPCODE_REPLY, 0x0a000002, 0x0a000001));
    interface.tick(30000);
    interface.send_datagram(datagram(0x0a000002), Address{"10.0.0.2"});
    require(port->frames.size() == 1 && port->frames.back().header.type == EthernetHeader::TYPE_ARP,
            "ARP entry did not expire at 30 seconds");

    port->frames.clear();
    interface.tick(5000);
    interface.send_datagram(datagram(0x0a000002), Address{"10.0.0.2"});
    require(port->frames.size() == 1 && port->frames.back().header.type == EthernetHeader::TYPE_ARP,
            "ARP request was still suppressed at five seconds");
    port->frames.clear();
    interface.recv_frame(arp(ARPMessage::OPCODE_REPLY, 0x0a000002, 0x0a000001));
    require(port->frames.size() == 1 && port->frames.back().header.type == EthernetHeader::TYPE_IPv4,
            "expired pending datagram was retained");
}

void route_prefixes() {
    auto default_port = make_shared<RecordingPort>();
    auto specific_port = make_shared<RecordingPort>();
    auto default_interface = make_shared<NetworkInterface>("default", default_port, local_mac, Address{"10.0.0.1"});
    auto specific_interface = make_shared<NetworkInterface>("specific", specific_port, local_mac, Address{"10.0.0.1"});
    default_interface->recv_frame(arp(ARPMessage::OPCODE_REPLY, 0x0a000002, 0x0a000001));
    specific_interface->recv_frame(arp(ARPMessage::OPCODE_REPLY, 0x0a000002, 0x0a000001));

    Router router;
    const size_t default_id = router.add_interface(default_interface);
    const size_t specific_id = router.add_interface(specific_interface);
    router.add_route(0, 0, Address{"10.0.0.2"}, default_id);
    router.add_route(0x80000001, 1, Address{"10.0.0.2"}, specific_id);
    router.add_route(0x0a026304, 16, Address{"10.0.0.2"}, specific_id);

    for (const uint32_t destination : {uint32_t{0x0a020163}, uint32_t{0xc6336407}}) {
        default_interface->datagrams_received().push(datagram(destination));
        router.route();
        require(default_port->frames.empty() && specific_port->frames.size() == 1, "route prefix was not masked");
        InternetDatagram forwarded;
        require(parse(forwarded, specific_port->frames.back().payload), "forwarded checksum was invalid");
        require(forwarded.header.ttl == 63 && forwarded.header.dst == destination, "forwarded header was incorrect");
        specific_port->frames.clear();
    }
}

void unwrap_full_range() {
    const Wrap32 isn{137};
    const uint64_t maximum = numeric_limits<uint64_t>::max();
    for (const uint64_t checkpoint : {uint64_t{1} << 63, maximum - 16, maximum}) {
        const auto wrapped = Wrap32::wrap(checkpoint, isn);
        require(wrapped.unwrap(isn, checkpoint) == checkpoint, "unwrap failed above the signed 64-bit range");
    }
    require(Wrap32{0}.unwrap(Wrap32{0}, maximum) == maximum - UINT32_MAX, "unwrap crossed UINT64_MAX");
}

int main() {
    const array tests{pair{"reassembler EOF", reassembler_eof},
                      pair{"sender shrinking window", sender_window_shrink},
                      pair{"sender idle timer", sender_idle_timer},
                      pair{"sender zero window", sender_zero_window},
                      pair{"receiver initial sequence", receiver_initial_sequence},
                      pair{"ARP learning", arp_request_learning},
                      pair{"ARP expiration", arp_expiration},
                      pair{"route prefixes", route_prefixes},
                      pair{"unwrap full range", unwrap_full_range}};
    bool passed = true;
    for (const auto &[name, test] : tests) {
        try {
            test();
        } catch (const exception &error) {
            cerr << name << ": " << error.what() << '\n';
            passed = false;
        }
    }
    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}

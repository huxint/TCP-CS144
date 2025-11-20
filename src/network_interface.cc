#include <cstdint>
#include <iostream>

#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
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

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    EthernetFrame message;
    uint32_t target_ip = next_hop.ipv4_numeric();
    message.header.src = ethernet_address_;          // 本机的MAC
    message.header.type = EthernetHeader::TYPE_IPv4; // 这是 IPv4 帧
    message.payload = serialize(dgram);              // IP 数据报序列化到 payload

    if (!arp_table_.contains(target_ip) || arp_table_[target_ip].second < current_time_) {
        // 没记录，或者记录已过期
        /**
        把这个尚未填 dst 的 IPv4 帧丢进 frame_queue_[target_ip].first
        的队列里。然后构造一个空frame，调用send_arp_request(target_ip, arp_request_frame) 去广播 ARP 请求。*/
        frame_queue_[target_ip].first.push(std::move(message));
        EthernetFrame arp_request_frame;
        send_arp_request(target_ip, arp_request_frame);
        return;
    } else {
        // 有效记录
        message.header.dst = arp_table_[target_ip].first; // 就是mac地址
        transmit(message);
    }
}

void NetworkInterface::send_arp_request(const uint32_t target_ip, EthernetFrame &arp_request_frame) {
    if (frame_queue_.contains(target_ip) && frame_queue_[target_ip].second.has_value() &&
        frame_queue_[target_ip].second >= current_time_) {
        return;
    }
    /**
    如果 `frame_queue_[target_ip].second` 里存了一个时间 `T`，并且 `T >= current_time_`，表示从现在到 T
    之前，已经发过一次 ARP 请求了，不要再发，直接 `return`。
    */

    // 构造以太网帧头：广播 ARP 请求
    arp_request_frame.header.type = EthernetHeader::TYPE_ARP; // ARP 帧
    arp_request_frame.header.dst = ETHERNET_BROADCAST;        // 广播
    arp_request_frame.header.src = ethernet_address_;         // 自己的 MAC

    // 构造 ARP 消息体
    ARPMessage arp_request_message;
    arp_request_message.sender_ethernet_address = ethernet_address_;    // 我的mac
    arp_request_message.sender_ip_address = ip_address_.ipv4_numeric(); // 我的ip
    arp_request_message.opcode = ARPMessage::OPCODE_REQUEST;            // 请求
    arp_request_message.target_ip_address = target_ip;                  // 我要问的ip
    // arp_request_message.target_ethernet_address = ETHERNET_BROADCAST;  // 这个可以空着
    arp_request_frame.payload = serialize(arp_request_message); // 序列化到 payload
    transmit(arp_request_frame);
    frame_queue_[target_ip].second = current_time_ + 5000; // 设置冷却时间
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header.dst == ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST) {
        if (frame.header.type == EthernetHeader::TYPE_ARP) { // 处理 ARP 帧
            ARPMessage message;
            if (parse(message, frame.payload) && message.target_ip_address == ip_address_.ipv4_numeric()) {
                arp_table_[message.sender_ip_address] =
                    std::make_pair(message.sender_ethernet_address, current_time_ + 30000);
                if (message.opcode == ARPMessage::OPCODE_REQUEST) {
                    EthernetFrame response;
                    make_arp_response(message, response);
                    transmit(response);
                } else {
                    // 收到arp回复之后看对应ip有无待发送的消息
                    queue<EthernetFrame> &ip_queue = frame_queue_[message.sender_ip_address].first;
                    while (!ip_queue.empty()) {
                        ip_queue.front().header.dst = message.sender_ethernet_address;
                        transmit(ip_queue.front());
                        ip_queue.pop();
                    }
                }
            }
        } else if (frame.header.type == EthernetHeader::TYPE_IPv4) { // 处理 IPv4 帧
            InternetDatagram message;
            if (parse(message, frame.payload)) { // 解析成功，就放到接收队列里
                datagrams_received_.emplace(std::move(message));
            }
        }
    }
}
void NetworkInterface::make_arp_response(const ARPMessage &message, EthernetFrame &response) const {
    EthernetHeader &header = response.header;
    header.dst = message.sender_ethernet_address;
    header.src = ethernet_address_;
    header.type = EthernetHeader::TYPE_ARP;
    ARPMessage arp_response_message = ARPMessage();
    arp_response_message.opcode = ARPMessage::OPCODE_REPLY;
    arp_response_message.sender_ethernet_address = ethernet_address_;
    arp_response_message.sender_ip_address = ip_address_.ipv4_numeric();
    arp_response_message.target_ethernet_address = message.sender_ethernet_address;
    arp_response_message.target_ip_address = message.sender_ip_address;
    response.payload = serialize(arp_response_message);
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    current_time_ += ms_since_last_tick;
}

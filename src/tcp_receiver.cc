#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <limits>

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message) {
    if (message.RST) {
        reader().set_error();
        return;
    }

    if (message.SYN) {
        zero_point_ = message.seqno;
        ack_.emplace(static_cast<Wrap32>(true)); // 设置ackno为有值
    }

    if (ack_.has_value()) {
        uint64_t check_point = writer().bytes_pushed() + 1; // 已发送的字节数 + 1，因为syn占用了一个字节
        uint64_t first_index = Wrap32(message.seqno + message.SYN).unwrap(zero_point_, check_point) - 1;
        // 这一段中是否为开头的那一段，是就说明有syn，需要+1，然后找到离check_point最近的index，-1代表是流的下标
        reassembler_.insert(first_index, std::move(message.payload), message.FIN);
        ack_.emplace(ack_->wrap(writer().bytes_pushed() + 1 + writer().is_closed(), zero_point_));
        // 更新ack
    }
}

TCPReceiverMessage TCPReceiver::send() const {
    return {ack_,
            static_cast<uint16_t>(std::min(reassembler_.writer().available_capacity(),
                                           static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()))),
            reader().has_error()};
}

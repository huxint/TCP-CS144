#include "tcp_receiver.hh"

#include <algorithm>
#include <limits>

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message) {
    if (message.RST) {
        reader().set_error();
    } else {
        if (!isn_ && message.SYN) {
            isn_ = message.seqno;
        }
        if (isn_ && (!message.SYN || message.seqno == *isn_)) {
            const uint64_t first = message.seqno.unwrap(*isn_, writer().bytes_pushed() + 1) + message.SYN;
            const uint64_t end = first + message.payload.size();
            // Stream bytes occupy sequence positions [1, end); SYN occupies position zero.
            if (end >= 1) {
                const uint64_t begin = max<uint64_t>(first, 1);
                message.payload.erase(0, begin - first);
                reassembler_.insert(begin - 1, std::move(message.payload), message.FIN);
            }
        }
    }
}

TCPReceiverMessage TCPReceiver::send() const {
    optional<Wrap32> ackno;
    if (isn_) {
        ackno = Wrap32::wrap(writer().bytes_pushed() + 1 + writer().is_closed(), *isn_);
    }
    return {ackno,
            static_cast<uint16_t>(min<uint64_t>(writer().available_capacity(), numeric_limits<uint16_t>::max())),
            reader().has_error()};
}

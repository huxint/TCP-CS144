#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>
#include <utility>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const {
    return send_count_ - ack_count_;
}

uint64_t TCPSender::consecutive_retransmissions() const {
    return retx_count_;
}

void TCPSender::push(const TransmitFunction &transmit) {
    const uint64_t window = max<uint64_t>(windows_size_, 1);
    auto &input = input_.reader();
    while (!fin_ && sequence_numbers_in_flight() < window &&
           (!syn_ || input.bytes_buffered() != 0 || input.is_finished())) {
        const uint64_t remaining = window - sequence_numbers_in_flight();
        auto message = make_empty_message();
        message.SYN = !syn_;
        syn_ = true;

        const uint64_t length = min({TCPConfig::MAX_PAYLOAD_SIZE,
                                     remaining - message.sequence_length(),
                                     input.bytes_buffered()});
        message.payload.reserve(length);
        while (message.payload.size() < length) {
            const auto data = input.peek().substr(0, length - message.payload.size());
            message.payload.append(data);
            input.pop(data.size());
        }

        if (input.is_finished() && message.sequence_length() < remaining) {
            message.FIN = true;
            fin_ = true;
        }
        transmit(message);
        if (retx_queue_.empty()) {
            timer_ = 0;
        }
        send_count_ += message.sequence_length();
        retx_queue_.push({std::move(message), send_count_});
    }
}

TCPSenderMessage TCPSender::make_empty_message() const {
    return {.seqno = Wrap32::wrap(send_count_, isn_), .RST = input_.has_error()};
}

void TCPSender::receive(const TCPReceiverMessage &message) {
    windows_size_ = message.window_size;
    if (message.RST) {
        input_.set_error();
    } else if (message.ackno) {
        const uint64_t acknowledged = message.ackno->unwrap(isn_, ack_count_);
        if (ack_count_ < acknowledged && acknowledged <= send_count_) {
            ack_count_ = acknowledged;
            while (!retx_queue_.empty() && retx_queue_.front().end_seqno <= ack_count_) {
                retx_queue_.pop();
            }
            retx_count_ = 0;
            RTO_ms_ = initial_RTO_ms_;
            timer_ = 0;
        }
    }
}

void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction &transmit) {
    if (!retx_queue_.empty()) {
        timer_ += ms_since_last_tick;
        if (timer_ >= RTO_ms_) {
            transmit(retx_queue_.front().message);
            if (windows_size_ != 0) {
                ++retx_count_;
                RTO_ms_ *= 2;
            }
            timer_ = 0;
        }
    }
}

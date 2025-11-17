#include "tcp_sender.hh"
#include "tcp_config.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <string>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const { // 未确认的字节数，就是发送出去的减去已经确认了的
    return send_count_ - ack_count_;
}

uint64_t TCPSender::consecutive_retransmissions() const { // 返回连续重传报文的数目
    return retx_count_;
}

void TCPSender::push(const TransmitFunction &transmit) { // 从内存字节流中读取待发送数据，尽可能填满接收窗口
    uint64_t remaining; // remaining：当前窗口还能承载的序号数量 = 窗口 - 未确认的。
    while ((remaining = (windows_size_ == 0 ? 1 : windows_size_) - sequence_numbers_in_flight()) > 0 && !fin_) {
        // 窗口不为 0 时，尽可能多的发送数据, 如果已经发过 FIN，就直接结束

        auto msg = make_empty_message();
        if (!syn_) {
            msg.SYN = true;
            syn_ = true;
            /*
            我们之前有没有已经发过 SYN。如果还没发过（第一次进入 push()，或者之前没机会发），这里就给这个 segment 加上
            SYN = true，并记下已经发过 SYN。
            */
        }

        uint64_t len = std::min(TCPConfig::MAX_PAYLOAD_SIZE, remaining - msg.sequence_length());
        // 限制上限

        auto &&data = msg.payload;
        while (reader().bytes_buffered() != 0 && data.size() < len) {
            // 因为要实现的是尽可能多的发送数据，所以要判断是否还有数据可读
            auto current_data = reader().peek().substr(0, len - data.size()); // 取当前可读数据的子串，长度为剩余空间
            /*
            注意，如何剩余空间比较大，就是整个字符串
            否则，就取当前可读数据的子串，长度为剩余空间
            */
            data += current_data;
            input_.reader().pop(current_data.size());
        }

        if (!fin_ && remaining > msg.sequence_length() && reader().is_finished()) {
            // 还剩至少 1 个序号空间，可以给 FIN 用
            // ByteStream 的写端已经关闭
            msg.FIN = true;
            fin_ = true;
        }

        if (msg.sequence_length() == 0) {
            break;
            /*
            这一轮你造出来的是一个“空壳”：啥也没装进去。如果这时候不 break，回到
            while顶端，很可能条件还成立，又进来，再造一个 msg，又啥都装不了，又回到这里……就会一直空跑循环。 */
        }

        transmit(msg);
        send_count_ += msg.sequence_length();
        retx_queue_.push(std::move(msg)); // 把这个报文段放到重传队列中，可能需要重传
    }
}

TCPSenderMessage TCPSender::make_empty_message() const { // 产生一条不占用序号的空消息
    // 以当前的发送进度为起点，先做一个啥都没有的空包壳子
    return {.seqno = Wrap32::wrap(send_count_, isn_), // 因为是不占用序号的空消息，所以序号就是当前发送的字节数
            .SYN = false,
            .payload = std::string{},
            .FIN = false,
            .RST = input_.has_error()};
}

void TCPSender::receive(const TCPReceiverMessage &msg) { // 接收来自接受者的确认消息，维护接收窗口的大小
    windows_size_ = msg.window_size;
    if (msg.RST) {
        input_.set_error();
        return;
    }

    if (msg.ackno.has_value()) {
        uint64_t recv_ackno = msg.ackno->unwrap(isn_, ack_count_);
        // 把 32 位的 ackno 展开成 64 位绝对序号。
        // 用 ack_count_ 作为 checkpoint，让展开的结果离当前已确认位置最近。

        if (recv_ackno > send_count_) {
            return; // Impossible ackno (beyond next seqno) is ignored
        }

        if (recv_ackno > ack_count_) { // 说明确实有新的数据要被确认
            ack_count_ = recv_ackno;   // 更新确认序号
            retx_count_ = 0; // 连续重传计数 retx_count_ 清零，因为新的 ACK 到来了，之前的超时重传序列就打断了
            RTO_ms_ = initial_RTO_ms_; // 重置 RTO
            if (sequence_numbers_in_flight() != 0) {
                timer_ = 0; // 重置定时器，因为有新的 ACK 到来，所以之前的超时重传序列就打断了
            }
        }
    }
}

void TCPSender::tick(uint64_t ms_since_last_tick,
                     const TransmitFunction &transmit) { // 根据外部传入的时间判断是否需要重传和进行拥塞控制
    timer_ += ms_since_last_tick;
    if (timer_ >= RTO_ms_) { // 超时重传
        while (!retx_queue_.empty()) {
            auto msg = retx_queue_.front();                    // 从重传队列中取出最旧的报文段
            auto sendno = msg.seqno.unwrap(isn_, send_count_); // 把 32 位的 seqno 展开成 64 位绝对序号。
            if (sendno + msg.sequence_length() > ack_count_) {
                transmit(msg);
                if (windows_size_ != 0) {
                    RTO_ms_ *= 2; // 如果超时重传了，还没收到 ACK，就把重传超时加倍，避免网络已经很拥塞还疯狂重发。
                }

                /*
                窗口为 0 的时候：超时时依然会重传（做 zero-window probe），只是不会把 RTO
                翻倍，而是用固定间隔探测窗口是否打开。
                你可以想象两种世界：
                    世界 A（当前代码）：
                        窗口 0 → 每隔固定 1 秒发一个探针包，问一句“你窗口开了吗？”
                    世界 B（如果去掉 if，总是乘 2）：
                        窗口 0 → 1秒、2秒、4秒、8秒、16秒... 才发一包
                        窗口关久一点，你几乎就不再去问了，一直等超长时间 😅
                */

                retx_count_++;
                timer_ = 0;
                break;
            } else {
                retx_queue_.pop(); // 小于等于说明这个报文段已经被确认了，不需要重传了
            }
        }
    }
}

#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity)
: capacity_(capacity) {}

bool Writer::is_closed() const {
    return close_;
}

void Writer::push(string data) {
    const uint64_t bytes = std::min(available_capacity(), data.size());
    if (bytes != 0) {
        bytes_pushed_ += bytes;
        bytes_buffer_ += bytes;
        buffer_.emplace(bytes == data.size() ? std::move(data) : data.substr(0, bytes));
        if (buffer_.size() == 1) {
            remaining_front_bytes = bytes;
        }
    }
}

void Writer::close() {
    close_ = true;
}

uint64_t Writer::available_capacity() const { // 剩余可用容量
    return capacity_ - bytes_buffer_;
}

uint64_t Writer::bytes_pushed() const { // 已写入字节数
    return bytes_pushed_;
}

bool Reader::is_finished() const {
    return close_ && bytes_buffer_ == 0;
}

uint64_t Reader::bytes_popped() const { // 已弹出字节数
    return bytes_popped_;
}

string_view Reader::peek() const {
    if (bytes_buffer_ == 0) {
        return string_view{};
    }
    const string &front = buffer_.front();
    return {front.data() + front.size() - remaining_front_bytes, remaining_front_bytes};
}

void Reader::pop(uint64_t len) {
    bytes_popped_ += len;
    bytes_buffer_ -= len;
    while (len != 0) {
        if (len >= remaining_front_bytes) {
            len -= remaining_front_bytes;
            buffer_.pop();
            remaining_front_bytes = buffer_.empty() ? 0 : buffer_.front().size();
        } else {
            remaining_front_bytes -= len;
            len = 0;
        }
    }
}

uint64_t Reader::bytes_buffered() const { // 缓冲区的字节数
    return bytes_buffer_;
}

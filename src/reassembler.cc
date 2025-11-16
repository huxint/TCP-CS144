#include "reassembler.hh"
#include <cstdint>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring) {
    uint64_t first_unassembled = writer().bytes_pushed();
    uint64_t first_unaccepted = first_unassembled + writer().available_capacity();

    if (first_index >= first_unaccepted || first_index + data.size() < first_unassembled) {
        return;
    }

    auto left = std::max(first_index, first_unassembled);
    auto right = std::min(first_index + data.size(), first_unaccepted);

    for (auto i = left; i < right; i++) {
        if (auto idx = i - first_unassembled; !flags_[idx]) {
            buffer_[idx] = data[i - first_index];
            unassembled_bytes_++;
            flags_[idx] = true;
        }
    }

    std::string res;
    while (flags_.front()) {
        res += buffer_.front();
        buffer_.pop_front();
        flags_.pop_front();
        // 环形缓冲区，需要将空字符也入队占位
        buffer_.push_back('\0');
        flags_.push_back(false);
    }

    if (!res.empty()) {
        output_.writer().push(res);
        unassembled_bytes_ -= res.size();
    }

    if (is_last_substring) {
        eof_ = true;
        eof_index_ = first_index + data.size();
    }

    if (eof_ && eof_index_ == writer().bytes_pushed()) {
        output_.writer().close();
    }
}

uint64_t Reassembler::bytes_pending() const {
    return unassembled_bytes_;
}

#include "reassembler.hh"
#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring) {
    uint64_t capacity = writer().available_capacity();
    uint64_t bytes_pushed = writer().bytes_pushed();
    uint64_t left = std::max(first_index, bytes_pushed);
    uint64_t right = std::min(first_index + data.size(), bytes_pushed + capacity);
    if (left > right) { // 相等的时候为空串，也能接受（可能标志了is_last_substring）
        return;
    }

    Segment segment{.eof = is_last_substring && right == first_index + data.size(),
                    .left = left,
                    .right = right,
                    .data = data.substr(left - first_index, right - left)};
    pending_size_ += segment.data.size(); // 先全部加上

    auto iterator = std::lower_bound(buffer_.begin(), buffer_.end(), segment);

    auto merge = [&](Segment &lhs, const Segment &rhs) {
        if (lhs.right < rhs.right) {
            auto length = lhs.right - rhs.left;
            lhs.data += rhs.data.substr(length); // 加上剩余的
            lhs.eof |= rhs.eof;
            lhs.right = rhs.right;
            pending_size_ -= length;
        } else {
            pending_size_ -= rhs.data.size();
        }
    };

    for (auto it = iterator; it != buffer_.end() && segment.right >= it->left; it = buffer_.erase(it)) {
        merge(segment, *it);
    }

    if (iterator != buffer_.begin()) {
        if (auto it = std::prev(iterator); it->right >= segment.left) {
            merge(*it, segment);
            return;
        }
    }

    buffer_.insert(iterator, segment);

    if (auto &head = buffer_.front(); head.left == bytes_pushed) {
        output_.writer().push(head.data);
        pending_size_ -= head.data.size();
        if (head.eof) {
            output_.writer().close();
        }
        buffer_.erase(buffer_.begin());
    }
}

uint64_t Reassembler::bytes_pending() const {
    return pending_size_;
}

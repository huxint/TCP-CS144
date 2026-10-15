#include "reassembler.hh"

#include <algorithm>

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring) {
    auto &output = output_.writer();
    const uint64_t next_index = output.bytes_pushed();
    const uint64_t window_end = next_index + output.available_capacity();
    const uint64_t substring_end = first_index + data.size();
    if (is_last_substring && next_index <= substring_end && substring_end <= window_end) {
        end_index_ = substring_end;
    }

    uint64_t begin = max(first_index, next_index);
    uint64_t end = min(substring_end, window_end);
    if (begin < end) {
        auto it = buffer_.upper_bound(begin);
        if (it != buffer_.begin()) {
            const auto previous = std::prev(it);
            begin = max(begin, previous->first + previous->second.size());
        }

        // Keep overlapping boundary fragments and replace fully covered ones.
        while (it != buffer_.end() && it->first + it->second.size() <= end) {
            pending_size_ -= it->second.size();
            it = buffer_.erase(it);
        }
        if (it != buffer_.end()) {
            end = min(end, it->first);
        }

        if (begin < end) {
            string fragment = begin == first_index && end == substring_end
                                  ? std::move(data)
                                  : data.substr(begin - first_index, end - begin);
            if (begin == next_index) {
                output.push(std::move(fragment));
            } else {
                pending_size_ += fragment.size();
                buffer_.emplace_hint(it, begin, std::move(fragment));
            }
        }
    }

    for (auto it = buffer_.begin(); it != buffer_.end() && it->first == output.bytes_pushed(); it = buffer_.erase(it)) {
        pending_size_ -= it->second.size();
        output.push(std::move(it->second));
    }
    if (end_index_ == output.bytes_pushed()) {
        output.close();
    }
}

uint64_t Reassembler::bytes_pending() const {
    return pending_size_;
}

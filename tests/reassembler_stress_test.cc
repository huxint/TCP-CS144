#include "reassembler.hh"

#include <algorithm>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

void run(uint32_t seed) {
    std::mt19937 random(seed);
    const uint64_t capacity = 1 + random() % 1024;
    std::string expected(8192, '\0');
    for (auto &byte : expected) {
        byte = static_cast<char>(random() % 256);
    }
    Reassembler reassembler{ByteStream{capacity}};
    std::vector<bool> present(expected.size());
    uint64_t pushed = 0;
    uint64_t popped = 0;
    uint64_t pending = 0;
    std::optional<uint64_t> end_index;

    auto check = [&] {
        const bool closed = end_index == pushed;
        if (reassembler.writer().bytes_pushed() != pushed || reassembler.bytes_pending() != pending ||
            reassembler.reader().bytes_buffered() != pushed - popped ||
            reassembler.reader().bytes_popped() != popped ||
            reassembler.writer().available_capacity() != capacity - (pushed - popped) ||
            reassembler.writer().is_closed() != closed ||
            reassembler.reader().is_finished() != (closed && pushed == popped)) {
            throw std::runtime_error("state mismatch, seed=" + std::to_string(seed));
        }
    };

    auto insert = [&](uint64_t first, uint64_t length) {
        const uint64_t end = first + length;
        const bool last = end == expected.size();
        const uint64_t left = std::max(first, pushed);
        const uint64_t right = std::min(end, popped + capacity);
        if (left <= right) {
            if (last && right == end) {
                end_index = end;
            }
            for (uint64_t index = left; index < right; ++index) {
                if (!present[index]) {
                    present[index] = true;
                    ++pending;
                }
            }
            while (pushed < expected.size() && present[pushed]) {
                ++pushed;
                --pending;
            }
        }
        reassembler.insert(first, expected.substr(first, length), last);
        check();
    };

    auto pop = [&](uint64_t length) {
        while (length != 0) {
            const auto view = reassembler.reader().peek();
            const auto count = std::min<uint64_t>(length, view.size());
            if (count == 0 || view.substr(0, count) != std::string_view(expected).substr(popped, count)) {
                throw std::runtime_error("data mismatch, seed=" + std::to_string(seed));
            }
            reassembler.reader().pop(count);
            popped += count;
            length -= count;
        }
        check();
    };

    for (size_t step = 0; step < 1000; ++step) {
        uint64_t first = pushed;
        switch (random() % 3) {
        case 0:
            first = random() % (expected.size() + 1);
            break;
        case 1:
            first = std::min<uint64_t>((pushed > capacity ? pushed - capacity : 0) +
                                          random() % (3 * capacity + 1),
                                      expected.size());
            break;
        }
        const uint64_t length = std::min<uint64_t>(random() % (2 * capacity + 65), expected.size() - first);
        insert(first, length);
        pop(random() % (pushed - popped + 1));
    }
    while (popped != expected.size()) {
        insert(pushed, expected.size() - pushed);
        pop(pushed - popped);
    }
}

int main() {
    for (uint32_t seed = 0; seed < 100; ++seed) {
        run(seed);
    }
    Reassembler empty{ByteStream{0}};
    empty.insert(0, "", true);
    if (!empty.reader().is_finished() || empty.bytes_pending() != 0) {
        throw std::runtime_error("empty stream mismatch");
    }
    std::cout << "100 seeds x 1000 random insert/pop steps: PASS\n";
}

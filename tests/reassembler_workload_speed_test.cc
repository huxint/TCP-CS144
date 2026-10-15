#include "reassembler.hh"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

struct Packet {
    uint64_t index;
    std::string data;
    bool last;
};

void run(const std::string &kind, size_t count, size_t rounds) {
    const bool holes = kind == "holes" || kind == "random_holes";
    const size_t width = holes ? 2 * count + 1 : count * 1500;
    const size_t capacity = kind == "in_order" ? 1500 : width;
    std::string expected(width * rounds, '\0');
    for (size_t i = 0; i < expected.size(); ++i) {
        expected[i] = static_cast<char>((i * 17 + i / 251) % 256);
    }

    std::vector<Packet> packets;
    packets.reserve((count + 1) * rounds);
    std::mt19937 random(144);
    for (size_t round = 0; round < rounds; ++round) {
        const size_t base = round * width;
        if (holes) {
            std::vector<size_t> order(count);
            std::iota(order.begin(), order.end(), 0);
            if (kind == "random_holes") {
                std::shuffle(order.begin(), order.end(), random);
            }
            for (const size_t i : order) {
                const size_t index = base + 2 * i + 1;
                packets.push_back({index, expected.substr(index, 1), false});
            }
            packets.push_back({base, expected.substr(base, width), round + 1 == rounds});
        } else {
            for (size_t i = 0; i < count; ++i) {
                const size_t index = base + (kind == "reverse" ? count - 1 - i : i) * 1500;
                packets.push_back({index, expected.substr(index, 1500), index + 1500 == expected.size()});
            }
        }
    }

    Reassembler reassembler{ByteStream{capacity}};
    std::string output;
    output.reserve(expected.size());
    const auto start = std::chrono::steady_clock::now();
    for (auto &packet : packets) {
        reassembler.insert(packet.index, std::move(packet.data), packet.last);
        while (reassembler.reader().bytes_buffered()) {
            const auto view = reassembler.reader().peek();
            output.append(view);
            reassembler.reader().pop(view.size());
        }
    }
    const auto stop = std::chrono::steady_clock::now();
    if (output != expected || !reassembler.reader().is_finished() || reassembler.bytes_pending() != 0) {
        throw std::runtime_error("incorrect result: " + kind);
    }
    const double seconds = std::chrono::duration<double>(stop - start).count();
    std::cout << kind << ',' << count << ',' << rounds << ',' << seconds * 1000 / rounds << ','
              << expected.size() * 8 / seconds / 1e9 << '\n';
}

int main() {
    run("in_order", 20000, 1);
    run("reverse", 64, 32);
    run("reverse", 256, 16);
    run("reverse", 1024, 4);
    run("holes", 256, 32);
    run("holes", 1024, 8);
    run("holes", 4096, 4);
    run("random_holes", 4096, 4);
}

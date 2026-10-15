#include "tcp_peer.hh"

#include <algorithm>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

string payload(size_t length, size_t seed) {
    string data(length, '\0');
    for (size_t i = 0; i < length; ++i) {
        data[i] = static_cast<char>((i * 17 + i / 251 + seed) % 256);
    }
    return data;
}

void transfer() {
    TCPConfig client_config;
    client_config.isn = Wrap32{UINT32_MAX - 15};
    client_config.rt_timeout = 10;
    client_config.send_capacity = 8192;
    client_config.recv_capacity = 4096;
    TCPConfig server_config = client_config;
    server_config.isn = Wrap32{UINT32_MAX - 7};
    server_config.recv_capacity = 2048;
    TCPPeer client{client_config};
    TCPPeer server{server_config};

    const string request = payload(256 * 1024, 7);
    const string response = payload(128 * 1024, 13);
    size_t request_written = 0;
    size_t response_written = 0;
    string request_received;
    string response_received;
    request_received.reserve(request.size());
    response_received.reserve(response.size());

    deque<TCPMessage> to_client;
    deque<TCPMessage> to_server;
    size_t transmitted = 0;
    size_t dropped = 0;
    size_t duplicated = 0;
    auto enqueue = [&](deque<TCPMessage> &queue, TCPMessage message) {
        ++transmitted;
        if (transmitted % 11 == 0) {
            ++dropped;
            return;
        }
        if (transmitted % 17 == 0) {
            queue.push_back(message);
            ++duplicated;
        }
        queue.push_back(std::move(message));
    };
    const TCPPeer::TransmitFunction client_transmit = [&](TCPMessage message) {
        enqueue(to_server, std::move(message));
    };
    const TCPPeer::TransmitFunction server_transmit = [&](TCPMessage message) {
        enqueue(to_client, std::move(message));
    };

    auto write = [](TCPPeer &peer, const string &data, size_t &written, const auto &transmit) {
        auto &writer = peer.outbound_writer();
        const size_t count = min<size_t>(writer.available_capacity(), data.size() - written);
        if (count != 0) {
            writer.push(data.substr(written, count));
            written += count;
        }
        if (written == data.size()) {
            writer.close();
        }
        peer.push(transmit);
    };
    auto deliver = [](deque<TCPMessage> &queue, TCPPeer &peer, const auto &transmit, bool newest) {
        if (queue.empty()) {
            return;
        }
        TCPMessage message;
        if (newest) {
            message = std::move(queue.back());
            queue.pop_back();
        } else {
            message = std::move(queue.front());
            queue.pop_front();
        }
        peer.receive(std::move(message), transmit);
    };
    auto drain = [](Reader &reader, string &received, size_t budget) {
        while (reader.bytes_buffered() != 0 && budget != 0) {
            const auto view = reader.peek().substr(0, budget);
            received.append(view);
            reader.pop(view.size());
            budget -= view.size();
        }
    };

    bool zero_window_seen = false;
    bool finished = false;
    for (size_t time = 0; time < 20000; ++time) {
        write(client, request, request_written, client_transmit);
        write(server, response, response_written, server_transmit);
        deliver(to_server, server, server_transmit, time % 3 == 0);
        deliver(to_client, client, client_transmit, time % 3 == 1);
        zero_window_seen |= server.receiver().send().window_size == 0;

        // Delay reads, then consume slowly enough to exercise window updates.
        if (time >= 200 && time % 5 == 0) {
            drain(server.inbound_reader(), request_received, 257);
        }
        if (time >= 100 && time % 3 == 0) {
            drain(client.inbound_reader(), response_received, 541);
        }
        client.tick(1, client_transmit);
        server.tick(1, server_transmit);

        if (client.inbound_reader().is_finished() && server.inbound_reader().is_finished() &&
            client.sender().sequence_numbers_in_flight() == 0 && server.sender().sequence_numbers_in_flight() == 0 &&
            to_client.empty() && to_server.empty()) {
            finished = true;
            break;
        }
    }

    if (!finished || request_received != request || response_received != response || !zero_window_seen ||
        dropped == 0 || duplicated == 0 || client.inbound_reader().has_error() || server.inbound_reader().has_error()) {
        throw runtime_error("bidirectional transfer did not recover from loss, reordering, and a zero window");
    }
    client.tick(101, client_transmit);
    server.tick(101, server_transmit);
    if (client.active() || server.active()) {
        throw runtime_error("peers did not close after the final ACK and linger interval");
    }
    cout << "Transferred " << request.size() + response.size() << " bytes, with " << dropped
         << " dropped and " << duplicated << " duplicated messages.\n";
}

int main() {
    try {
        transfer();
    } catch (const exception &error) {
        cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

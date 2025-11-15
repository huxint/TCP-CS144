#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <queue>

class Reader;
class Writer;

class ByteStream {
public:
    explicit ByteStream(uint64_t capacity);

    // Helper functions (provided) to access the ByteStream's Reader and Writer interfaces
    Reader &reader();
    const Reader &reader() const;
    Writer &writer();
    const Writer &writer() const;

    void set_error() {
        error_ = true;
    }; // Signal that the stream suffered an error.
    bool has_error() const {
        return error_;
    }; // Has the stream had an error?

    uint64_t capacity() const {
        return capacity_;
    }

protected:
    // Please add any additional state to the ByteStream here, and not to the Writer and Reader interfaces.
    uint64_t capacity_;
    bool error_{};

    std::queue<std::string> buffer_{}; // 缓冲区
    uint64_t bytes_buffer_{};          // 当前缓冲区字节数
    uint64_t bytes_pushed_{};          // 总写入量
    uint64_t bytes_popped_{};          // 总读取量
    uint64_t remaining_front_bytes{};  // 第一个字符串剩余字节数
    bool close_{};                     // 端口状态
};

class Writer : public ByteStream {
public:
    void push(std::string data); // Push data to stream, but only as much as available capacity allows.
    void close();                // Signal that the stream has reached its ending. Nothing more will be written.

    bool is_closed() const;              // Has the stream been closed?
    uint64_t available_capacity() const; // How many bytes can be pushed to the stream right now?
    uint64_t bytes_pushed() const;       // Total number of bytes cumulatively pushed to the stream
};

class Reader : public ByteStream {
public:
    std::string_view peek() const; // Peek at the next bytes in the buffer
    void pop(uint64_t len);        // Remove `len` bytes from the buffer

    bool is_finished() const;        // Is the stream finished (closed and fully popped)?
    uint64_t bytes_buffered() const; // Number of bytes currently buffered (pushed and not popped)
    uint64_t bytes_popped() const;   // Total number of bytes cumulatively popped from stream
};

/*
 * read: A (provided) helper function thats peeks and pops up to `len` bytes
 * from a ByteStream Reader into a string;
 */
void read(Reader &reader, uint64_t len, std::string &out);

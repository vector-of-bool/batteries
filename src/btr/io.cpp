#include "./io.hpp"

using namespace btr;

std::string byte_io_stream::read() {
    // Start with 4kb
    std::string ret;
    ret.resize(1024 * 4);
    // Keep track of how far into 'ret' we have read
    std::size_t offset = 0;
    while (true) {
        // The number of bytes remaining in 'ret'
        const auto remaining = ret.size() - offset;
        // The destination for the next read operation
        const auto ptr = ret.data() + offset;
        // Read!
        const auto nread = read_into(ptr, remaining);
        // Advance the offset
        offset += nread;
        if (nread < remaining) {
            // We have read to the end
            break;
        }
        // We may have more to read. Double the buffer size
        ret.resize(ret.size() * 2);
    }
    // Shrink back down to how much we actually read
    ret.resize(offset);
    return ret;
}

std::string byte_io_stream::read(std::size_t count) {
    std::string ret;
    ret.resize(count);
    auto nread = read_into(ret);
    ret.resize(nread);
    return ret;
}

std::u8string byte_io_stream::u8read(std::size_t count) {
    std::u8string ret;
    ret.resize(count);
    auto nread = read_into(ret);
    ret.resize(nread);
    return ret;
}

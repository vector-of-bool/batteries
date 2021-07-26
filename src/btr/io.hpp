#pragma once

#include "./trivial_range.hpp"

#include <cstdint>
#include <string>

namespace btr {

/**
 * @brief Abstract base class of objects used for byte-stream-oriented I/O
 */
class byte_io_stream {
public:
    virtual ~byte_io_stream() = default;

private:
    /// Provided by concrete derived classes
    virtual std::size_t do_read_into(mutable_buffer buf) = 0;
    /// Provided by concrete derived classes
    virtual std::size_t do_write(const_buffer buf) = 0;

public:
    /**
     * @brief Read some data/objects from the file
     *
     * @tparam T A trivially-copyable type to read from the file
     * @param out The pointer-to-T instances where data will be placed
     * @param count The number of T objects that are pointed-to by `out`
     * @return std::size_t The number of T objects that were read
     */
    template <trivial_type T>
    std::size_t read_into(T* out, std::size_t count) {
        auto nbytes = do_read_into(neo::mutable_buffer(neo::byte_pointer(out), count * sizeof(T)));
        return nbytes / sizeof(T);
    }

    /**
     * @brief Read into the given contiguous range.
     *
     * @param out A contiguous range of trivially-copyable objects where the data will be stored
     * @return std::size_t The number of *objects* that were read
     *
     * @note Attempts to read 'size(out)' objects.
     */
    std::size_t read_into(mutable_trivial_range auto&& range) {
        return do_read_into(mutable_buffer(range)) / neo::data_type_size_v<decltype(range)>;
    }

    /**
     * @brief Read all data until the end-of-file/end-of-stream condition is hit
     *
     * @return std::string A string containing the contents that were read
     */
    std::string read();

    /**
     * @brief Read *at most* `count` bytes.
     *
     * @param count The number of bytes (char) to read, *at most*
     * @return std::string The bytes that were read from the file
     *
     * @note The returned string will be *at most* `count` bytes long, and will
     *       be resized down to fit the amount of data that was actually read.
     */
    std::string read(std::size_t count);

    /**
     * @brief Read a UTF-8 string from.
     *
     * @param codeunits The number of code units to read.
     * @return std::u8string A UTF-8 stream. May be a truncated UTF-8 string!
     */
    std::u8string u8read(std::size_t codeunits);

    /**
     * @brief Write the given data into the file
     *
     * @param data The data to be written
     * @returns The number of *objects* that were written
     */
    std::size_t write(trivial_range auto&& data) {
        auto nbytes = do_write(const_buffer(data));
        return nbytes / neo::data_type_size_v<decltype(data)>;
    }
};

}  // namespace btr

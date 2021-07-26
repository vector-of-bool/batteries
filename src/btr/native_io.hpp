#pragma once

#include <neo/platform.hpp>

#include "./io.hpp"

namespace btr {

/**
 * @brief Traits for a Win32 HANDLE-based object
 *
 */
struct win32_handle_traits {
    using handle_type = void*;

    inline static const handle_type null_handle = static_cast<char*>(nullptr) - 1;

    static void        close(handle_type) noexcept;
    static std::size_t write(handle_type h, const_buffer);
    static std::size_t read(handle_type h, mutable_buffer);
};

/**
 * @brief Traits for a POSIX file descriptor
 */
struct posix_fd_traits {
    using handle_type = int;

    inline static const handle_type null_handle = -1;

    static void        close(handle_type) noexcept;
    static std::size_t write(handle_type, const_buffer);
    static std::size_t read(handle_type, mutable_buffer);
};

/**
 * @brief A handle-managing IO stream.
 *
 * @tparam Traits The traits of the handle type, including how to read, write, and close it
 */
template <typename Traits>
class handle_io_stream : public byte_io_stream {
public:
    using handle_type = typename Traits::handle_type;

    inline static const handle_type null_handle = Traits::null_handle;

private:
    handle_type _handle = null_handle;

public:
    /**
     * @brief Default-contsruct a null (unopened) stream
     */
    handle_io_stream() = default;
    /// Move from another stream
    handle_io_stream(handle_io_stream&& other) noexcept { reset(other.release()); }
    /// Move-assign from another stream
    handle_io_stream& operator=(handle_io_stream&& o) noexcept {
        reset(o.release());
        return *this;
    }

    explicit handle_io_stream(handle_type&& h) noexcept { reset(NEO_MOVE(h)); }

    /// Destroys and closes the stream
    ~handle_io_stream() { reset(handle_type{null_handle}); }

    /**
     * @brief Obtain a copy of the handle managed by this object
     */
    [[nodiscard]] handle_type get() const noexcept { return _handle; }

    /// Determine whether this stream is open for I/O
    [[nodiscard]] bool is_open() const noexcept { return get() != this->null_handle; }

    /// Close and reset the stream
    void close() noexcept {
        Traits::close(get());
        _handle = null_handle;
    }

    /**
     * @brief Replace the handle managed by this object
     *
     * @param h A handle to swap into place. The given handle will be reset to a null value in the
     * caller's scope.
     */
    void reset(handle_type&& h) noexcept {
        if (is_open()) {
            close();
        }
        _handle = h;
    }

    /**
     * @brief Relinquish ownership of the managed handle and return it to the caller.
     *
     * @note It is the duty of the caller to ensure the returned handle will be closed properly
     */
    [[nodiscard]] handle_type release() noexcept {
        auto h  = _handle;
        _handle = null_handle;
        return h;
    }

private:
    std::size_t do_write(const_buffer cbuf) override {
        auto n = Traits::write(get(), cbuf);
        if (n == 0) {
            close();
        }
        return n;
    }
    std::size_t do_read_into(mutable_buffer mbuf) override {
        auto n = Traits::read(get(), mbuf);
        if (n == 0) {
            close();
        }
        return n;
    }
};

/**
 * @brief A handle-owning IO stream for the current platform.
 */
struct native_io_stream
    : handle_io_stream<
          std::conditional_t<neo::os_is_windows, win32_handle_traits, posix_fd_traits>> {
    using handle_io_stream::handle_io_stream;
};

/**
 * @brief A non-owning reference to a native handle byte stream.
 */
class native_io_stream_ref : native_io_stream {
public:
    native_io_stream_ref(handle_type h)
        : native_io_stream(NEO_MOVE(h)) {}
    ~native_io_stream_ref() { (void)release(); }

    native_io_stream_ref(const native_io_stream& io)
        : native_io_stream(io.get()) {}

    using native_io_stream::get;
    using native_io_stream::is_open;
    using native_io_stream::read;
    using native_io_stream::read_into;
    using native_io_stream::write;
};

}  // namespace btr

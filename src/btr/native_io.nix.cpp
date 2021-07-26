#include "./native_io.hpp"

#include "./syserror.hpp"

using namespace btr;

#if !_WIN32

#include <unistd.h>

void posix_fd_traits::close(int fd) noexcept { ::close(fd); }

std::size_t posix_fd_traits::write(int fd, const_buffer cbuf) {
    neo_assert(expects,
               fd != null_handle,
               "Attempted to write data to a closed file descriptor",
               std::string_view(cbuf),
               cbuf.size());
    auto nwritten = ::write(fd, cbuf.data(), cbuf.size());
    if (nwritten < 0) {
        throw_current_error("::write() on file descriptor failed");
    }
    return static_cast<std::size_t>(nwritten);
}

std::size_t posix_fd_traits::read(int fd, mutable_buffer buf) {
    neo_assert(expects,
               fd != null_handle,
               "Attempted to read data from a closed file descriptor",
               buf.size());
    auto nread = ::read(fd, buf.data(), buf.size());
    if (nread < 0) {
        throw_current_error("::read() on file descriptor failed");
    }
    return static_cast<std::size_t>(nread);
}

#endif

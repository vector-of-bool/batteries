#include "./native_io.hpp"

#include "./syserror.hpp"

using namespace btr;

#if _WIN32

#include <windows.h>

void win32_handle_traits::close(HANDLE h) { ::CloseHandle(h); }

std::size_t win32_handle_traits::write(HANDLE h, const_buffer buf) {
    neo_assert(expects,
               h != null_handle,
               "Attempted to write data to a closed HANDLE",
               std::string_view(buf),
               buf.size());
    DWORD nwritten = 0;
    auto  okay     = ::WriteFile(h, buf.data(), static_cast<DWORD>(buf.size()), &nwritten, nullptr);
    if (!okay) {
        throw_current_error("::WriteFile() failed");
    }
    return static_cast<std::size_t>(nwritten);
}

std::size_t win32_handle_traits::read(HANDLE h, mutable_buffer buf) {
    neo_assert(expects,
               h != null_handle,
               "Attempted to read data from a closed HANDLE",
               buf.size());
    DWORD nread = 0;
    auto  okay  = ::ReadFile(h, buf, static_cast<DWORD>(buf.size()), &nread, nullptr);
    if (!okay) {
        throw_current_error("::ReadFile() failed");
    }
    return static_cast<std::size_t>(nread);
}

#endif

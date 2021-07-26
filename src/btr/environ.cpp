#include "./environ.hpp"

#include "./utf.hpp"

using namespace btr;

#if !_WIN32

std::optional<std::string> btr::getenv(u8view key) noexcept {
    auto ptr = std::getenv(key.data());
    if (ptr) {
        return std::make_optional(std::string(ptr));
    } else {
        return std::nullopt;
    }
}

std::optional<std::u8string> btr::u8getenv(u8view key) noexcept {
    auto ptr = std::getenv(key.data());
    if (ptr) {
        return std::u8string(u8view(ptr));
    } else {
        return std::nullopt;
    }
}

#else

#include <windows.h>

namespace {

std::optional<std::wstring> getenv_wstr(std::wstring const& varname, std::size_t size_hint = 256) {
    std::wstring ret;
    ret.resize(size_hint);
    while (true) {
        auto real_len
            = ::GetEnvironmentVariableW(varname.data(), ret.data(), static_cast<DWORD>(ret.size()));
        if (real_len == 0 && ::GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            // Environment variable is not defined
            return std::nullopt;
        } else if (real_len > size_hint) {
            // Try again, with a larger buffer
            ret.resize(real_len);
            continue;
        } else {
            // Got it!
            ret.resize(real_len);
            return ret;
        }
    }
    return std::make_optional(std::move(ret));
}

}  // namespace

std::optional<std::string> btr::getenv(u8view key) noexcept {
    std::optional<std::wstring> val = getenv_wstr(btr::wide_encode(key));
    if (!val) {
        return std::nullopt;
    }
    return btr::u8_as_char_encode(*val);
}

std::optional<std::u8string> btr::u8getenv(u8view key) noexcept {
    auto val = getenv_wstr(btr::wide_encode(key.u8string_view()));
    if (!val) {
        return std::nullopt;
    }
    return btr::u8encode(*val);
}

#endif

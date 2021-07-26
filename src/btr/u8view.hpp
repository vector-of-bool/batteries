#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace btr {

/**
 * @brief A string-view like type that accepts either a std::string[_view] or
 * a std::u8string[_view]. This will implicitly convert from either argument.
 *
 * Intended for use to bridge the gap between char8_t users and char users
 * that assume 'char' to be UTF-8.
 */
class u8view {
    std::u8string_view _view;

public:
    /// Convert from a std::string
    template <typename Traits, typename Alloc>
    constexpr u8view(std::basic_string<char, Traits, Alloc> const& str) noexcept
        : _view(reinterpret_cast<const char8_t*>(str.data()), str.size()) {}

    /// Convert from a std::string_view
    template <typename Traits>
    constexpr u8view(std::basic_string_view<char, Traits> sv) noexcept
        : _view(reinterpret_cast<const char8_t*>(sv.data()), sv.size()) {}

    /// Convert from a std::u8string
    template <typename Traits, typename Alloc>
    constexpr u8view(std::basic_string<char8_t, Traits, Alloc> const& str) noexcept
        : _view(str.data(), str.size()) {}

    /// Convert from a std::u8string_view
    template <typename Traits>
    constexpr u8view(std::basic_string_view<char8_t, Traits> sv) noexcept
        : _view(sv.data(), sv.size()) {}

    /// Convert from a string literal
    u8view(const char* ptr) noexcept
        : _view(reinterpret_cast<const char8_t*>(ptr)) {}

    /// Convert from a u8 string literal
    constexpr u8view(const char8_t* ptr) noexcept
        : _view(ptr) {}

    /// Construct from a char-pointer + length
    u8view(const char* ptr, std::size_t length) noexcept
        : _view(reinterpret_cast<const char8_t*>(ptr), length) {}

    /// Construct from a char8_t-pointer + length
    constexpr u8view(const char8_t* ptr, std::size_t length) noexcept
        : _view(ptr, length) {}

    /// Return as a std::u8string_view
    constexpr std::u8string_view u8string_view() const noexcept { return _view; }
    /// Return as a std::string_view
    std::string_view string_view() const noexcept {
        return std::string_view(reinterpret_cast<const char*>(_view.data()), _view.size());
    }

    /// Implicitly convert to any std::basic_string_view who has a character
    /// size of 1 byte
    template <typename Char, typename Traits>
    requires(sizeof(Char) == 1) operator std::basic_string_view<Char, Traits>() const noexcept {
        return std::basic_string_view<Char, Traits>(reinterpret_cast<const Char*>(_view.data()),
                                                    _view.size());
    }

    /// Explicitly convert to a std::basic_string with a character type of size 1
    template <typename Char, typename Traits, typename Alloc>
    requires(sizeof(Char) == 1) explicit operator std::basic_string<Char, Traits>() const noexcept {
        return std::basic_string<Char, Traits, Alloc>(reinterpret_cast<const Char*>(_view.data()),
                                                      _view.size());
    }

    /// Get a pointer to the char8_t data
    constexpr const char8_t* u8data() const noexcept { return _view.data(); }
    /// Get a pointer to char data
    const char* data() const noexcept { return reinterpret_cast<const char*>(u8data()); }

    /// Obtain the number of bytes in the viewed string
    constexpr std::size_t size_bytes() const noexcept { return _view.size(); }
};

}  // namespace btr

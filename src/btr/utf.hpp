#pragma once

#include <neo/fwd.hpp>
#include <neo/iterator_facade.hpp>
#include <neo/tag.hpp>

#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace btr {

using neo::tag;
using neo::tag_v;
struct u8_as_char {};

struct utf_decode_error : std::runtime_error {
    using runtime_error::runtime_error;
};

namespace utf_detail {

struct ll_decode_res {
    char32_t    codepoint;
    std::size_t n_cus_taken;
};

inline ll_decode_res ll_decode(char32_t const* ptr, const char32_t*) noexcept {
    return ll_decode_res{*ptr, 1};
}

ll_decode_res ll_decode(const char16_t* ptr, const char16_t* stop);
ll_decode_res ll_decode(const char8_t* ptr, const char8_t* stop);
ll_decode_res ll_decode(const char* ptr, const char* stop);
ll_decode_res ll_decode(const wchar_t* ptr, const wchar_t* stop);

template <typename Char>
struct ll_encode_res {
    Char        units[4];
    std::size_t count;
};

inline ll_encode_res<char32_t> ll_encode(char32_t c, neo::tag<char32_t>) noexcept {
    return {{c}, 1};
}

ll_encode_res<char16_t> ll_encode(char32_t c, neo::tag<char16_t>);
ll_encode_res<char8_t>  ll_encode(char32_t c, neo::tag<char8_t>);
ll_encode_res<char>     ll_encode(char32_t c, neo::tag<char>);
ll_encode_res<wchar_t>  ll_encode(char32_t c, neo::tag<wchar_t>);

inline bool ll_is_start_cu(char32_t&) noexcept { return true; }

}  // namespace utf_detail

/**
 * @brief Result of a single decode_one() operation
 *
 * @tparam Iter The iterator in the range of code units that was given
 */
template <typename Iter>
struct decode_one_result {
    /// The decoded Unicode codepoint
    char32_t codepoint;
    /// Iterator past the end of the final decoded code unit
    Iter input;
};

/**
 * @brief Decode a Unicode code point from the given range of code units
 *
 * @param iter The beginning of a code unit range
 * @param stop The end of the code unit range
 * @return decode_one_result<Iter>
 */
template <std::contiguous_iterator Iter>
decode_one_result<Iter> decode_one(Iter iter, Iter stop) {
    // A contiguous iterator lets us call the low-level decode operations directly.
    auto addr       = std::to_address(iter);
    auto len        = static_cast<std::size_t>(stop - iter);
    auto [cp, dist] = utf_detail::ll_decode(addr, addr + len);
    return decode_one_result<Iter>{cp, iter + dist};
}

/**
 * @brief Decode a Unicode code point from the given range of code units
 *
 * @param it The beginning of a code unit range
 * @param stop The end of the code unit range
 * @return decode_one_result<Iter>
 */
template <std::input_iterator Iter, std::sentinel_for<Iter> Stop>
decode_one_result<Iter> decode_one(Iter it, Stop stop) {
    using cp_type   = std::iter_value_t<Iter>;
    cp_type buf[4]  = {};
    auto    buf_end = std::ranges::copy(it, stop, buf).out;
    auto    n_cps   = static_cast<std::size_t>(buf_end - buf);
    return decode_one(buf, buf_end);
}

/**
 * @brief Decode a Unicode code point from the given range of code units
 *
 * @param r A range of code units. The first code point in the range will be decoded
 * @return decode_one_result<std::ranges::iterator_t<R>>
 */
template <std::ranges::input_range R>
decode_one_result<std::ranges::iterator_t<R>> decode_one(R&& r) {
    return decode_one(std::ranges::begin(r), std::ranges::end(r));
}

/**
 * @brief Create a range that decodes and iterates Unicode codepoints from a range of some UTF
 * encoded range
 *
 * @tparam Range Any common input range of a character type.
 */
template <std::ranges::input_range Range>
requires std::ranges::common_range<Range>  //
    class codepoint_range {
    Range _range;
    using inner_iter_t = std::ranges::iterator_t<Range>;

public:
    using code_unit = std::ranges::range_value_t<Range>;

    explicit codepoint_range(Range&& r)
        : _range(NEO_FWD(r)) {}

    class iterator : public neo::iterator_facade<iterator> {
        inner_iter_t _iter;
        inner_iter_t _stop;

        decode_one_result<inner_iter_t> _result;

    public:
        explicit iterator(inner_iter_t it, inner_iter_t stop)
            : _iter(it)
            , _stop(stop) {
            if (it != stop) {
                _result = decode_one(_iter, stop);
            }
        }

        char32_t dereference() const noexcept { return _result.codepoint; }
        void     increment() {
            _iter = _result.input;
            if (_iter != _stop) {
                _result = decode_one(_iter, _stop);
            }
        }

        constexpr bool operator==(iterator o) const noexcept { return o._iter == _iter; }
    };

    constexpr iterator begin() noexcept {
        return iterator{std::ranges::begin(_range), std::ranges::end(_range)};
    }

    constexpr iterator end() noexcept {
        return iterator{std::ranges::end(_range), std::ranges::end(_range)};
    }
};

template <std::ranges::input_range R>
explicit codepoint_range(R &&) -> codepoint_range<R>;

/**
 * @brief Transcode a string from one Unicode encoding to another
 *
 * The input encoding is infered from the value_type of the input range. The
 * output encoding is infered from the character type given as the explicit
 * template parameter as:
 *
 * - char - Returns std::string as UTF-8 text
 * - char8_t - Returns std::u8string as UTF-8 text
 * - char16_t - Returns std::u16string as UTF-16 text
 * - char32_t - Returns std::u32string as UTF-32 text
 * - wchar_t - Returns a std::wstring using the system's wide-encoding
 *             (UTF-16 on Windows, UTF-32 elsewhere).
 *
 * @tparam CharOut The output character type.
 * @param rng A string to convert from. Must have a character value type.
 * @returns A new std::basic_string of the appropriate type
 */
template <typename CharOut, std::ranges::input_range Range>
decltype(auto) transcode_string(Range&& rng) {
    codepoint_range            codepoints{rng};
    std::basic_string<CharOut> str;
    for (char32_t cp : codepoints) {
        auto enc = utf_detail::ll_encode(cp, tag_v<CharOut>);
        str.append(enc.units, enc.count);
    }
    return str;
}

/// Encoding the given string as a UTF-8 std::u8string
std::u8string u8encode(std::ranges::contiguous_range auto&& rng) {
    return transcode_string<char8_t>(NEO_FWD(rng));
}

/// Encode the given string as a UTF-8 string in a std::string
std::string u8_as_char_encode(std::ranges::contiguous_range auto&& rng) {
    return transcode_string<u8_as_char>(NEO_FWD(rng));
}

/// Encode the given string as a UTF-16 std::u16string
std::u16string u16encode(std::ranges::contiguous_range auto&& rng) {
    return transcode_string<char16_t>(NEO_FWD(rng));
}

/// Encoding the given string as a UTF-32 std::u32stirng
std::u32string u32encode(std::ranges::contiguous_range auto&& rng) {
    return transcode_string<char32_t>(NEO_FWD(rng));
}

/// Encode the given string as a wide-character std::wstring
std::wstring wide_encode(std::ranges::contiguous_range auto&& rng) {
    return transcode_string<wchar_t>(NEO_FWD(rng));
}

}  // namespace btr

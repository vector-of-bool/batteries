#include "./utf.hpp"

#include <neo/utf8.hpp>
#include <neo/utility.hpp>

using namespace btr;
using std::span;

utf_detail::ll_decode_res utf_detail::ll_decode(const char16_t* in, const char16_t* stop) {
    neo_assert(invariant, in != stop, "decode_one(char16_t) called with empty range");
    char16_t val = *in;
    if (neo::between(val, 0xd800, 0xdbff)) {
        // We need to decode surrogate pair
        ++in;
        if (in == stop) {
            throw utf_decode_error("Incomplete surrogate pair in UTF-16 stream");
        }
        char16_t first  = val;
        char16_t second = *in;
        if (!neo::between(second, 0xdc00, 0xdfff)) {
            throw utf_decode_error("Invalid surrogate pair value in UTF-16 sequence");
        }

        char16_t high = first - 0xd800;
        char16_t low  = second - 0xdc00;
        char32_t ret  = high;
        ret <<= 10;
        ret |= low;
        return {ret, 2};
    } else if (neo::between(val, 0x00, 0xd7ff) || neo::between(val, 0xe000, 0xffff)) {
        return {val, 1};
    } else {
        throw utf_decode_error("Invalid code unit in UTF-16 stream");
    }
}

utf_detail::ll_decode_res utf_detail::ll_decode(const wchar_t* in, const wchar_t* stop) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t) || sizeof(wchar_t) == sizeof(char32_t));
    if constexpr (sizeof(wchar_t) == sizeof(char32_t)) {
        return {static_cast<char32_t>(*in), 1};
    } else {
        return utf_detail::ll_decode(reinterpret_cast<const char16_t*>(in),
                                     reinterpret_cast<const char16_t*>(stop));
    }
}

utf_detail::ll_decode_res utf_detail::ll_decode(const char8_t* in, const char8_t* stop) {
    neo_assert(expects, in != stop, "Empty range of UTF-8 codepoints given to decode");
    auto cp   = neo::next_utf8_codepoint(in, stop);
    using err = neo::utf8_errc;
    switch (cp.error()) {
    case err::none:
        break;
    case err::invalid_continuation_byte:
        throw utf_decode_error("Invalid continuation byte in UTF-8 stream");
    case err::invalid_start_byte:
        throw utf_decode_error("Invalid codepoint start byte in UTF-8 stream");
    case err::need_more:
        throw utf_decode_error("Truncated UTF-8 encoded text");
    }
    return {cp.codepoint, cp.size};
}

utf_detail::ll_decode_res utf_detail::ll_decode(const char* ptr, const char* stop) {
    return utf_detail::ll_decode(reinterpret_cast<const char8_t*>(ptr),
                                 reinterpret_cast<const char8_t*>(stop));
}

namespace {

template <typename Char>
std::u32string decode_it(span<const Char> in) {
    std::u32string ret;
    while (!in.empty()) {
        auto res = btr::decode_one(in);
        ret.push_back(res.codepoint);
        in = span<const Char>{res.input, in.end()};
    }
    return ret;
}

}  // namespace

utf_detail::ll_encode_res<char> utf_detail::ll_encode(char32_t c, tag<char>) {
    auto                u8 = ll_encode(c, tag_v<char8_t>);
    ll_encode_res<char> r;
    std::ranges::copy(u8.units, r.units);
    r.count = u8.count;
    return r;
}

utf_detail::ll_encode_res<char8_t> utf_detail::ll_encode(char32_t c, tag<char8_t>) {
    ll_encode_res<char8_t> ret;
    auto                   out = ret.units;
    if (neo::between(c, 0, 0x7f)) {
        *out++ = static_cast<char8_t>(c);
    } else if (neo::between(c, 0x80, 0x7ff)) {
        *out++ = static_cast<char8_t>((0b11'000'000 | (c >> 6)));
        *out++ = static_cast<char8_t>((0b00'111'111 & c) | 0b10'000000);
    } else if (neo::between(c, 0x800, 0xffff)) {
        *out++ = static_cast<char8_t>((0b1110'0000) | (c >> 12));
        *out++ = static_cast<char8_t>((0b00'111'111 & (c >> 6)) | 0b10'000000);
        *out++ = static_cast<char8_t>((0b00'111'111 & (c >> 0)) | 0b10'000000);
    } else if (neo::between(c, 0x10'000, 0x10f'fff)) {
        *out++ = static_cast<char8_t>((0b1110'0000) | (c >> 18));
        *out++ = static_cast<char8_t>((0b00'111'111 & (c >> 12)) | 0b10'000000);
        *out++ = static_cast<char8_t>((0b00'111'111 & (c >> 06)) | 0b10'000000);
        *out++ = static_cast<char8_t>((0b00'111'111 & (c >> 00)) | 0b10'000000);
    } else {
        throw utf_decode_error("Invalid Unicode codepoint");
    }
    ret.count = static_cast<std::size_t>(out - ret.units);
    return ret;
}

utf_detail::ll_encode_res<char16_t> utf_detail::ll_encode(char32_t c, tag<char16_t>) {
    utf_detail::ll_encode_res<char16_t> ret;

    auto out = ret.units;
    if (neo::between(c, 0x00, 0xd7ff) || neo::between(c, 0xe000, 0xffff)) {
        *out++ = static_cast<char16_t>(c);
    } else if (neo::between(c, 0x10'000, 0x10f'fff)) {
        char32_t uprime = c - 0x10'000;
        auto     hi     = uprime >> 10;
        auto     lo     = uprime & ((1u << 11) - 1u);
        char16_t w1     = 0xd800 + static_cast<char16_t>(hi);
        char16_t w2     = 0xdc00 + static_cast<char16_t>(lo);
        *out++          = w1;
        *out++          = w2;
    } else {
        throw utf_decode_error{"Invalid Unicode codepoint"};
    }
    ret.count = static_cast<std::size_t>(out - ret.units);
    return ret;
}

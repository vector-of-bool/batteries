#pragma once

#include <neo/const_buffer.hpp>
#include <neo/mutable_buffer.hpp>
#include <neo/trivial_range.hpp>

#include <ranges>

namespace btr {

using neo::const_buffer;
using neo::mutable_buffer;
using neo::mutable_trivial_range;
using neo::trivial_range;
using neo::trivial_type;

constexpr std::size_t trivial_range_size_bytes(trivial_range auto&& r) noexcept {
    return neo::trivial_range_byte_size(r);
}

/**
 * @brief Determine whether the given argument is an array
 *
 * @return true if @tparam R is an array
 * @return false Otherwise
 */
template <typename R>
constexpr bool is_array(R&&) noexcept {
    return std::is_array_v<std::remove_cvref_t<R>>;
}

}  // namespace btr

#pragma once

#include "./u8view.hpp"
#include "./utf.hpp"

#include <memory>
#include <ranges>
#include <stdexcept>

namespace btr {

/**
 * @brief Exception thrown if we attempt to compile a bad fnmatch pattern
 */
class bad_fnmatch_pattern : public std::runtime_error {
    std::string _pat;
    std::string _reason;

public:
    /**
     * @brief Create a new exception
     *
     * @param pattern The pattern that is malformed
     * @param reason The reason that the pattern is malformed
     */
    bad_fnmatch_pattern(std::string pattern, std::string reason) noexcept;

    /// The pattern given to compile
    const std::string& pattern() const noexcept { return _pat; }
    /// The reason that the pattern is malformed
    const std::string& reason() const noexcept { return _reason; }
};

/**
 * @brief A pre-compiled fnmatch pattern
 */
class fnmatch_pattern {
    class impl;
    std::shared_ptr<const impl> _impl;

    bool _test(u8view) const noexcept;

    fnmatch_pattern(std::shared_ptr<const impl> ptr)
        : _impl(ptr) {}

public:
    /**
     * @brief Test whether the given string matches the compiled fnmatch pattern.
     *
     * @param string Any string or string-like range.
     * @return true If the string matches the pattern
     * @return false Otherwise
     */
    template <typename String>
    requires std::ranges::input_range<String> or std::convertible_to<String, u8view> //
    [[nodiscard]] bool test(String&& string) const {
        if constexpr (std::convertible_to<String, u8view>) {
            return _test(string);
        } else {
            return _test(btr::u8encode(string));
        }
    }

    /// Get the original spelling of the pattern
    [[nodiscard]] const std::string& literal_spelling() const noexcept;

    /// Compile the given fnmatch pattern
    [[nodiscard]] static fnmatch_pattern compile(u8view fnmatch_pattern);
};

/**
 * @brief Test whether the given @param string matches @param pattern
 *
 * @param pattern The pattern to check against.
 * @param string The string to test
 * @return true If the string matches the pattern
 * @return false Otherwise
 *
 * @note To reduce overhead of compiling and parsing `pattern` repeatedly, use
 *      `btr::fnmatch_pattern::compile()` to pre-compile the pattern.
 */
[[nodiscard]] inline bool fnmatch(u8view pattern, u8view string) {
    return fnmatch_pattern::compile(pattern).test(string.u8string_view());
}

}  // namespace btr

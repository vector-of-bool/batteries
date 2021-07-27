#pragma once

#include "./u8view.hpp"

#include <system_error>

namespace btr {

/// Clear the current OS error code
void clear_current_error() noexcept;
/// Set the current OS error code
void set_current_error(int error) noexcept;
/// Get the current OS error code
[[nodiscard]] int get_current_error() noexcept;
/// Get the current OS error code wrapped in a std::error_code
[[nodiscard]] std::error_code get_current_error_code() noexcept;

/**
 * @brief Throw a std::system_error that contains the given OS error code and the associated string
 * message
 *
 * @param code An OS-level error code number
 * @param message A message to include in the exception
 */
[[noreturn]] void throw_for_system_error_code(int code, u8view message);

/**
 * @brief Throw a std::system_error for the current OS error code, using the associated string
 * message
 *
 * @param message A string message to include in the exception
 */
[[noreturn]] void throw_current_error(u8view message);

/**
 * @brief If the current OS error is non-zero, throw a std::system_error for that error code and use
 * the given string message
 *
 * @param message
 */
void throw_if_current_error(u8view message);

}  // namespace btr

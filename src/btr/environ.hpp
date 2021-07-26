#pragma once

#include "./u8view.hpp"
#include "./utf.hpp"

#include <optional>
#include <string>

namespace btr {

/**
 * @brief Get the environment variable named by the given key
 *
 * @param key The name of an environment variable to get
 */
std::optional<std::string> getenv(u8view key) noexcept;

/**
 * @brief Get an environment variable value as a UTF-8 encoded string
 *
 * @param key The name of the variable to get
 * @return std::optional<std::u8string>
 */
std::optional<std::u8string> u8getenv(u8view key) noexcept;

}  // namespace btr

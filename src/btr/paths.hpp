#pragma once

#include <filesystem>

namespace btr {

/**
 * @brief Get the current user's home directory path
 */
const std::filesystem::path& user_home_dir() noexcept;

/**
 * @brief Get the path to a directory where applications should store their users' data.
 */
const std::filesystem::path& user_data_dir() noexcept;

/**
 * @brief Get the path to a directory where applications should store their cache data.
 */
const std::filesystem::path& user_cache_dir() noexcept;

/**
 * @brief Get the path to a directory where applications should store user-specific persistent
 * configuration
 */
const std::filesystem::path& user_config_dir() noexcept;

}  // namespace btr

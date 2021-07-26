#include "./paths.hpp"

#include "./environ.hpp"

#include <neo/platform.hpp>

using namespace btr;
namespace fs = std::filesystem;

namespace {

template <typename Func>
fs::path env_path_or(std::string_view key, Func&& fn) {
    auto found = btr::getenv(key);
    if (found) {
        return fs::absolute(*found);
    }
    return fn();
}

}  // namespace

const fs::path& btr::user_home_dir() noexcept {
    static auto ret = []() -> fs::path {
        if (neo::os_is_unix_like) {
            return env_path_or("HOME", [] { return "/"; });
        } else if (neo::os_is_windows) {
            return env_path_or("UserProfile", [] { return "/"; });
        } else {
            return "/";
        }
    }();
    return ret;
}

const fs::path& btr::user_data_dir() noexcept {
    static auto ret = []() -> fs::path {
        if (neo::os_is_unix_like) {
            return env_path_or("XDG_DATA_HOME", [] {
                if (neo::os_is_macos) {
                    return user_home_dir() / "Library/Application Support";
                } else {
                    return user_home_dir() / ".local/share";
                }
            });
        } else if (neo::os_is_windows) {
            return env_path_or("LocalAppData", [] { return "/"; });
        } else {
            return "/";
        }
    }();
    return ret;
}

const fs::path& btr::user_cache_dir() noexcept {
    static auto ret = []() -> fs::path {
        if (neo::os_is_unix_like) {
            return env_path_or("XDG_DATA_HOME", [] {
                if (neo::os_is_macos) {
                    return user_home_dir() / "Library/Caches";
                } else {
                    return user_home_dir() / ".cache";
                }
            });
        } else if (neo::os_is_windows) {
            return env_path_or("LocalAppData", [] { return "/"; });
        } else {
            return "/";
        }
    }();
    return ret;
}

const fs::path& btr::user_config_dir() noexcept {
    static auto ret = []() -> fs::path {
        if (neo::os_is_unix_like) {
            return env_path_or("XDG_DATA_HOME", [] {
                if (neo::os_is_macos) {
                    return user_home_dir() / "Library/Preferences";
                } else {
                    return user_home_dir() / ".cache";
                }
            });
        } else if (neo::os_is_windows) {
            return env_path_or("AppData", [] { return "/"; });
        } else {
            return "/";
        }
    }();
    return ret;
}

#pragma once

#include <filesystem>

#include <neo/iterator_facade.hpp>

#include <memory>
#include <vector>

namespace btr {

/**
 * @brief An object that can be used to scan directories for files that match
 * a certain pattern.
 */
class glob {
    struct impl;
    std::shared_ptr<const impl> _impl;

public:
    /**
     * @brief A glob iterator (also a range with begin() and end()) for searching a directory
     */
    class iterator : public neo::iterator_facade<iterator> {
        struct state;
        std::shared_ptr<const glob::impl> _impl;
        std::shared_ptr<state>            _state;

        bool _done = true;

    public:
        iterator() = default;
        /// Begin searching `dirpath` using glob `glb`
        iterator(const glob& glb, const std::filesystem::path& dirpath);
        /// Obtian the current entry
        std::filesystem::directory_entry dereference() const;
        /// Advance to the next matching entry, or finish
        void increment();

        // A sentinel to signal the end-of-range
        struct sentinel_type {};

        bool operator==(sentinel_type) const noexcept { return at_end(); }
        /// Check whether we are finished with a search
        bool at_end() const noexcept { return _done; }

        /// Returns *this (for use with range-for and range-algorithms)
        iterator begin() const noexcept { return *this; }
        /// Return a sentinel (for use with range-for and range-algorithms)
        sentinel_type end() const noexcept { return {}; }

        /**
         * @brief Exhaust the iterator and collect all search results into a vector.
         *
         * @return std::vector<std::filesystem::directory_entry> All entries that were found
         *
         * @note This should be called immediately after constructing the iterator, or to_vector()
         * will return unspecified results
         */
        [[nodiscard]] std::vector<std::filesystem::directory_entry> to_vector();

        friend glob;
    };

    friend iterator::state;

    /**
     * @brief Compile a new filesystem-globbing pattern given with the path-pattern 'str'
     *
     * @param str A pattern to compile. Each path element of the pattern must be a valid fnmatch()
     * pattern or an rglob "**" pattern.
     * @return glob A compiled glob
     */
    [[nodiscard]] static glob compile(const std::filesystem::path& str);

    /**
     * @brief Test whether the given filesystem path would match the globbing pattern
     */
    [[nodiscard]] bool test(const std::filesystem::path& path) const noexcept;

    /**
     * @brief Create a new search iterator of the given directory path
     *
     * @param path A path to an existing directory from which the search will execute
     * @return iterator
     */
    [[nodiscard]] iterator search(const std::filesystem::path& path) const noexcept {
        return iterator(*this, path);
    }
};

}  // namespace btr

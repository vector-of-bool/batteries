#pragma once

#include "./io.hpp"
#include "./trivial_range.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

#include <neo/utility.hpp>

namespace btr {

/**
 * @brief Exception type thrown by file APIs
 *
 * Derives from std::system_error
 */
struct file_error : std::system_error {
    using system_error::system_error;
};

/**
 * @brief Exception thrown if an attempt is made to open a non-existent file,
 * and the caller did not request for it to be created automatically.
 *
 * Derives from file_error (and therefore also from std::system_error)
 */
struct file_not_found_error : file_error {
    using file_error::file_error;
};

/**
 * @brief A very simple file class, which can be used to read and write files.
 *
 * This is a pretty thin wrapper around <cstdio>'s std::fopen() and friends.
 */
class file : public byte_io_stream {
    /// The file that we own
    std::FILE* _file = nullptr;

    /// Init with a FILE
    file(std::FILE* f)
        : _file(f) {}

    /// Calls std::fclose();
    void _close() noexcept;

    /// Read content from the file into the given pointer
    std::size_t do_read_into(mutable_buffer buf) override;

    /// Write data into the file
    std::size_t do_write(const_buffer buf) override;

public:
    using byte_io_stream::read;
    using byte_io_stream::write;

    /// Close the file
    ~file() { close(); }

    file(file&& o) noexcept
        : _file(o._file) {
        o._file = nullptr;
    }

    file& operator=(file&& o) noexcept {
        close();
        _file   = o._file;
        o._file = nullptr;
        return *this;
    }

    /**
     * @brief Close the associated file. If the file was already closed, does
     * nothing.
     */
    void close() noexcept {
        if (_file) {
            _close();
        }
        _file = nullptr;
    }

    /**
     * @brief Open a file with the specified mode
     *
     * @param filepath The file to open
     * @param mode The mode string. Refer to std::fopen() for the syntax
     * @return file The opened file.
     *
     * @throws file_error if we are unable to open the file
     */
    static file open(const std::filesystem::path& filepath, const char* mode);

    /**
     * @brief Open a file for binary reading.
     *
     * @param filepath The file to open
     * @return file The opened file
     *
     * @throws file_error if we are unable to open the file
     *
     * @note Equivalent to 'open(filepath, "rb")'
     */
    static file open(const std::filesystem::path& filepath) { return open(filepath, "rb"); }

    /**
     * @brief Read the contents of the file at the designated path
     *
     * @param filepath The path to a file to read
     * @return std::string The contents of the file
     *
     * @note Equivalent to 'open(filepath).read()'
     */
    static std::string read(const std::filesystem::path& filepath);

    /**
     * @brief Write the given data into the file at the designated path
     *
     * @param filepath The path to a file that will be written
     * @param content The data to write into the file
     *
     * @note Equivalent to 'open(filepath, "wb").write(content)'
     */
    static void write(const std::filesystem::path& filepath, trivial_range auto&& content) {
        auto f = open(filepath, "wb");
        f.write(content);
    }
};

}  // namespace btr

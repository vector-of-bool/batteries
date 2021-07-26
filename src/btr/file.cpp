#include "./file.hpp"

#include <neo/assert.hpp>
#include <neo/ufmt.hpp>

using namespace btr;

using std::filesystem::path;

file file::open(const path& fpath, const char* openmode) {
    auto f = std::fopen(fpath.string().data(), openmode);
    if (!f) {
        auto ec = std::error_code{errno, std::system_category()};
        if (errno == ENOENT) {
            // Special exception for file-not-found
            throw file_not_found_error(ec,
                                       neo::ufmt("Cannot open non-existent file [{}] for '{}'",
                                                 fpath.string(),
                                                 openmode));
        }
        // General exception for other errors
        throw file_error(ec,
                         neo::ufmt("Failed to open file [{}] for '{}'", fpath.string(), openmode));
    }
    return file(f);
}

std::size_t file::do_read_into(mutable_buffer mbuf) {
    errno      = 0;
    auto nread = std::fread(mbuf.data(), 1, mbuf.size(), _file);
    if (errno) {
        throw file_error(std::error_code{errno, std::system_category()},
                         "Failed to read from file");
    }
    return nread;
}

void file::_close() noexcept { std::fclose(_file); }

std::size_t file::do_write(const_buffer buf) {
    errno               = 0;
    const auto nwritten = std::fwrite(buf.data(), 1, buf.size(), _file);
    if (errno) {
        throw file_error(std::error_code{errno, std::system_category()},
                         "Failure to write to file");
    }
    neo_assert(ensures,
               nwritten == buf.size(),
               "Not all of the string content was written to the file",
               nwritten,
               buf.size());
    return nwritten;
}

std::string file::read(std::filesystem::path const& filepath) {
    auto f = open(filepath);
    return f.read();
}

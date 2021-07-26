#pragma once

#include "./io.hpp"
#include "./native_io.hpp"
#include "./trivial_range.hpp"
#include "./u8view.hpp"

#include <neo/platform.hpp>

#include <cstdint>
#include <span>
#include <type_traits>

namespace btr {

/**
 * @brief A pipe endpoint that is open for reading
 */
struct pipe_reader : btr::native_io_stream {
    using native_io_stream::native_io_stream;

private:
    using byte_io_stream::write;
};

/**
 * @brief A pipe endpoint that is open for writing
 */
struct pipe_writer : btr::native_io_stream {
    using native_io_stream::native_io_stream;

private:
    using byte_io_stream::read;
    using byte_io_stream::read_into;
};

/**
 * @brief An aggregate of a pair of read and write endpoints of a new pipe
 */
struct pipe_pair {
    /// The read-end of the pipe
    pipe_reader reader;
    /// The write-end of the pipe
    pipe_writer writer;
};

/// Create a new anonymous IPC pipe within the current process
pipe_pair create_pipe();

}  // namespace btr

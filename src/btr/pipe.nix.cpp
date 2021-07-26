#include "./pipe.hpp"

#include "./syserror.hpp"

#include <neo/assert.hpp>

#include <utility>

#if !_WIN32

#include <unistd.h>

btr::pipe_pair btr::create_pipe() {
    int  p[2] = {};
    auto rc   = ::pipe(p);
    if (rc == -1) {
        throw_current_error("::pipe() failed in btr::create()");
    }
    btr::pipe_pair ret;
    ret.reader.reset(std::move(p[0]));
    ret.writer.reset(std::move(p[1]));
    return ret;
}

#endif

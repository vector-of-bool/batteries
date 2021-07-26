#include "./pipe.hpp"

#include "./syserror.hpp"

#include <neo/assert.hpp>

#if _WIN32

#include <windows.h>

using namespace btr;

pipe_pair btr::create_pipe() {
    ::SECURITY_ATTRIBUTES security = {};
    security.bInheritHandle        = TRUE;
    security.nLength               = sizeof security;
    security.lpSecurityDescriptor  = nullptr;
    HANDLE reader;
    HANDLE writer;
    auto   okay = ::CreatePipe(&reader, &writer, &security, 0);
    if (!okay) {
        throw_current_error("::CreatePipe() failed");
    }

    btr::pipe_pair pair;
    pair.reader.adopt(std::move(reader));
    pair.writer.adopt(std::move(writer));
    return pair;
}

#endif

#include "./subprocess.hpp"

#include "./environ.hpp"
#include "./syserror.hpp"
#include "./utf.hpp"

#include <neo/ad_hoc_range.hpp>
#include <neo/as_buffer.hpp>
#include <neo/overload.hpp>
#include <neo/scope.hpp>
#include <neo/ufmt.hpp>

using namespace btr;

#if _WIN32

#include <windows.h>

namespace {

pipe_writer setup_spawn_file_output(std::filesystem::path const& filepath) {
    auto h = ::CreateFileW(filepath.c_str(),
                           GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr,
                           CREATE_ALWAYS | TRUNCATE_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        throw_current_error("::CreateFile() failed for opening subprocess output");
    }
    ::SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0);
    return pipe_writer{std::move(h)};
}

std::wstring path_lookup(std::wstring_view program) {
    auto prog = std::filesystem::path(program);
    if (prog.has_parent_path()) {
        return prog.native();
    }
    auto path    = btr::getenv("PATH");
    auto pathext = btr::getenv("PATHEXT");
    if (not path or not pathext) {
        return prog.native();
    }
    std::string remain_path = *path;
    while (!remain_path.empty()) {
        const auto semi_pos = remain_path.find(';');
        const auto cand_dir = remain_path.substr(0, semi_pos);
        remain_path         = remain_path.substr(cand_dir.size());
        if (not remain_path.empty())
            remain_path.erase(remain_path.begin());
        std::string remain_ext    = *pathext;
        auto        cand_basename = cand_dir / prog;
        if (std::filesystem::is_regular_file(cand_basename)) {
            return cand_basename.native();
        }
        while (not remain_ext.empty()) {
            const auto semi_pos2 = remain_ext.find(';');
            const auto cand_ext  = remain_ext.substr(0, semi_pos2);
            remain_ext           = remain_ext.substr(cand_ext.size());
            if (not remain_ext.empty())
                remain_ext.erase(remain_ext.begin());
            auto candidate = cand_basename;
            candidate += cand_ext;
            if (std::filesystem::is_regular_file(candidate)) {
                return candidate.native();
            }
        }
    }
    return prog.native();
}

}  // namespace

struct subprocess::impl {
    ::PROCESS_INFORMATION proc_info = {};

    btr::pipe_reader stdout_pipe;
    btr::pipe_reader stderr_pipe;
    btr::pipe_writer stdin_pipe;

    ~impl() {
        ::CloseHandle(proc_info.hProcess);
        ::CloseHandle(proc_info.hThread);
    }
};

subprocess subprocess::spawn(const subprocess_spawn_options& opts) {
    auto cmd_str  = quote_argv_string(opts.command);
    auto cmd_wide = wide_encode(cmd_str);

    std::wstring program;
    if (opts.program) {
        program = opts.program->native();
    } else {
        neo_assert(expects,
                   !opts.command.empty(),
                   "btr::subprocess::spawn(): opts.command cannot be empty without providing "
                   "opts.program.");
        program = wide_encode(opts.command.front());
        if (opts.env_path_lookup) {
            program = path_lookup(program);
        }
    }

    auto imp = std::make_unique<impl>();

    pipe_writer stdout_writer;
    pipe_writer stderr_writer;
    pipe_reader stdin_reader;
    bool        stderr_to_stdout_ = false;

    std::visit(  //
        neo::overload{
            [&](stdio_pipe_t) {
                auto pipe        = create_pipe();
                imp->stdout_pipe = std::move(pipe.reader);
                stdout_writer    = std::move(pipe.writer);
            },
            [&](stdio_inherit_t) {},
            [&](const std::filesystem::path& filepath) {
                stdout_writer = setup_spawn_file_output(filepath);
            },
            [&](stdio_null_t) { stdout_writer = setup_spawn_file_output("NUL"); },
        },
        opts.stdout_);

    std::visit(  //
        neo::overload{
            [&](stdio_pipe_t) {
                auto pipe        = create_pipe();
                imp->stderr_pipe = std::move(pipe.reader);
                stderr_writer    = std::move(pipe.writer);
            },
            [&](stdio_inherit_t) {},
            [&](stderr_to_stdout_t) { stderr_to_stdout_ = true; },
            [&](const std::filesystem::path& filepath) {
                stderr_writer = setup_spawn_file_output(filepath);
            },
            [&](stdio_null_t) { stderr_writer = setup_spawn_file_output("NUL"); },
        },
        opts.stderr_);

    if (imp->stdout_pipe.is_open()) {
        ::SetHandleInformation(imp->stdout_pipe.get(), HANDLE_FLAG_INHERIT, 0);
    }
    if (imp->stderr_pipe.is_open()) {
        ::SetHandleInformation(imp->stderr_pipe.get(), HANDLE_FLAG_INHERIT, 0);
    }

    ::STARTUPINFOW startup_info = {};
    if (stdout_writer.is_open()) {
        startup_info.hStdOutput = stdout_writer.get();
    }
    if (stderr_writer.is_open()) {
        startup_info.hStdError = stderr_writer.get();
    } else if (stderr_to_stdout_) {
        if (stdout_writer.is_open()) {
            startup_info.hStdError = startup_info.hStdOutput;
        } else {
            startup_info.hStdError = ::GetStdHandle(STD_OUTPUT_HANDLE);
        }
    } else {
        // No special action
    }
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.cb      = sizeof startup_info;

    BOOL okay
        = ::CreateProcessW(program.data(),
                           cmd_wide.data(),
                           nullptr,
                           nullptr,
                           TRUE,
                           CREATE_NEW_PROCESS_GROUP,
                           nullptr,
                           opts.working_directory.value_or(std::filesystem::current_path()).c_str(),
                           &startup_info,
                           &imp->proc_info);

    if (!okay) {
        throw_current_error(neo::ufmt("::CreateProcessW() failed for [{}]", cmd_str));
    }

    return subprocess{imp.release()};
}

bool subprocess::_do_is_running() const {
    const auto ret = ::WaitForSingleObject(_impl->proc_info.hProcess, 0);
    return ret == WAIT_TIMEOUT;
}

void subprocess::_do_send_signal(int signum) {
    auto okay = ::GenerateConsoleCtrlEvent(signum, _impl->proc_info.dwProcessId);
    if (!okay) {
        throw_current_error("::GenerateConsoleCtrlEvent() failed");
    }
}

void subprocess::_do_close() noexcept {
    delete _impl;
    _impl = nullptr;
}

void subprocess::_do_join() {
    BOOL okay = ::WaitForSingleObject(_impl->proc_info.hProcess, INFINITE);
    if (okay) {
        throw_current_error("::WaitForSingleObject() failed in btr::subproces::join()");
    }
    DWORD rc = 0;
    okay     = ::GetExitCodeProcess(_impl->proc_info.hProcess, &rc);
    if (!okay) {
        throw_current_error("::GetExitCodeProcess() failed in btr::subprocess::join()");
    }
    _exit_result = subprocess_exit{.exit_code = static_cast<int>(rc)};
}

pipe_reader& subprocess::_do_get_stdout_pipe() const noexcept { return _impl->stdout_pipe; }
pipe_reader& subprocess::_do_get_stderr_pipe() const noexcept { return _impl->stderr_pipe; }
pipe_writer& subprocess::_do_get_stdin_pipe() const noexcept { return _impl->stdin_pipe; }

void subprocess::_do_read_output(subprocess_output& out, std::chrono::milliseconds timeout) {
    HANDLE handles[2];
    auto   hout = handles;
    if (has_stdout()) {
        *hout++ = _impl->stdout_pipe.get();
    }
    if (has_stderr()) {
        *hout++ = _impl->stderr_pipe.get();
    }
    auto n_hndls = hout - handles;

    auto result
        = ::WaitForMultipleObjects(static_cast<DWORD>(n_hndls),
                                   handles,
                                   FALSE,
                                   timeout.count() < 0 ? INFINITE
                                                       : static_cast<DWORD>(timeout.count()));

    if (result == WAIT_FAILED) {
        throw_current_error("::WaitForMultipleObjects() fialed in subprocess::read_output()");
    }

    if (result == WAIT_TIMEOUT) {
        return;
    }

    for (pipe_reader* pipe_ : {&_impl->stdout_pipe, &_impl->stderr_pipe}) {
        auto& pipe = *pipe_;
        if (!pipe.is_open()) {
            continue;
        }

        bool         is_stdout  = &pipe == &_impl->stdout_pipe;
        std::string& target     = is_stdout ? out.stdout_ : out.stderr_;
        const auto   start_size = target.size();
        target.resize(start_size + 1024);

        ::COMMTIMEOUTS prev_timeouts;
        ::GetCommTimeouts(pipe.get(), &prev_timeouts);
        neo_defer { ::SetCommTimeouts(pipe.get(), &prev_timeouts); };
        ::COMMTIMEOUTS zero_timeouts           = {};
        zero_timeouts.ReadTotalTimeoutConstant = 0;
        ::SetCommTimeouts(pipe.get(), &zero_timeouts);

        size_t nread = pipe.read_into(neo::as_buffer(target) + start_size);
        target.resize(start_size + nread);
        if (nread == 0) {
            // End-of-file
            pipe.close();
        }
    }
}

#endif

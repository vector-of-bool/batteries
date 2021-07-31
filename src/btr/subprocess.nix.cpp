#include "./subprocess.hpp"

#include "./pipe.hpp"
#include "./signal.hpp"
#include "./syserror.hpp"

#include <neo/ad_hoc_range.hpp>
#include <neo/as_buffer.hpp>
#include <neo/assert.hpp>
#include <neo/overload.hpp>
#include <neo/scope.hpp>
#include <neo/utility.hpp>

#if !_WIN32

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace btr;

namespace {

struct pipe_pair {
    int read;
    int write;
};

void set_cloexec(int fileno) {
    int flags = ::fcntl(fileno, F_GETFD);
    if (flags < 0) {
        throw_current_error("::fcntl(GETFD) failed in pipe::for_subprocess_comms()");
    }
    flags |= FD_CLOEXEC;
    int rc = ::fcntl(fileno, F_SETFD, flags);
    if (rc == -1) {
        throw_current_error("::fcntl(SETFD) failed in pipe::for_subprocess_comms()");
    }
}

[[nodiscard]] pipe_writer setup_spawn_file_output(std::filesystem::path const& filepath) {
    int fd = ::open(filepath.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0b110'100'100);
    if (fd < 0) {
        throw_current_error(
            neo::ufmt("Failed to open file [{}] for stdout for subprocess", filepath.string()));
    }
    return pipe_writer{std::move(fd)};
}

[[nodiscard]] pipe_reader setup_spawn_file_input(std::filesystem::path const& filepath) {
    int fd = ::open(filepath.string().c_str(), O_RDONLY);
    if (fd < 0) {
        throw_current_error(
            neo::ufmt("Failed to open file [{}] as stdin for the subprocess", filepath.string()));
    }
    return pipe_reader{std::move(fd)};
}

void throw_if_error_on_pipe(btr::pipe_reader& error_pipe) {
    int  child_errno = 0;
    auto nread       = error_pipe.read_into(neo::trivial_buffer(child_errno));
    if (nread == 0) {
        // No error
        return;
    }
    std::string message;
    message.resize(1024);
    nread = error_pipe.read_into(message);
    message.resize(nread);
    throw_for_system_error_code(child_errno, message);
}

}  // namespace

struct subprocess::impl {
    ::pid_t pid;

    btr::pipe_reader stdout_pipe;
    btr::pipe_reader stderr_pipe;
    btr::pipe_writer stdin_pipe;

    subprocess_spawn_options spawn_options;
};

subprocess subprocess::spawn(const subprocess_spawn_options& opts) {
    auto imp           = std::make_unique<impl>();
    imp->spawn_options = opts;
    // spawn() expects char pointers
    std::vector<char*> strings;
    for (std::string_view s : opts.command) {
        strings.push_back(const_cast<char*>(s.data()));
    }
    strings.push_back(nullptr);

    std::string program;
    if (opts.program) {
        program = *opts.program;
    } else {
        neo_assert(expects,
                   !opts.command.empty(),
                   "btr::subprocess::spawn(): opts.command cannot be empty without providing "
                   "opts.program.",
                   opts);
        program = opts.command.front();
    }

    std::string workdir = opts.working_directory.value_or(std::filesystem::current_path()).string();
    std::string chdir_error_message  = neo::ufmt("Failed to chdir() into directory [{}]", workdir);
    std::string execvp_error_message = neo::ufmt("execvp() failed for executable [{}]", program);

    btr::pipe_writer stdout_writer;
    btr::pipe_writer stderr_writer;
    btr::pipe_reader stdin_reader;

    bool stderr_to_stdout = false;

    std::visit(  //
        neo::overload{
            [&](stdio_pipe_t) {
                auto pipe        = create_pipe();
                imp->stdout_pipe = std::move(pipe.reader);
                stdout_writer    = std::move(pipe.writer);
            },
            [&](stdio_inherit_t) {
                // Do nothing. Child will inherit our stdout
            },
            [&](const std::filesystem::path& filepath) {
                stdout_writer = setup_spawn_file_output(filepath);
            },
            [&](stdio_null_t) { stdout_writer = setup_spawn_file_output("/dev/null"); },
        },
        opts.stdout_);

    std::visit(  //
        neo::overload{
            [&](stdio_pipe_t) {
                auto pipe        = create_pipe();
                imp->stderr_pipe = std::move(pipe.reader);
                stderr_writer    = std::move(pipe.writer);
            },
            [&](stdio_inherit_t) {
                // Do nothing. Child will inherit our stderr
            },
            [&](const std::filesystem::path& filepath) {
                stderr_writer = setup_spawn_file_output(filepath);
            },
            [&](stderr_to_stdout_t) { stderr_to_stdout = true; },
            [&](stdio_null_t) { stderr_writer = setup_spawn_file_output("/dev/null"); },
        },
        opts.stderr_);

    std::visit(  //
        neo::overload{
            [&](stdio_pipe_t) {
                auto pipe       = create_pipe();
                imp->stdin_pipe = std::move(pipe.writer);
                stdin_reader    = std::move(pipe.reader);
                set_cloexec(imp->stdin_pipe.get());
            },
            [&](stdio_inherit_t) {
                // Do nothing. Child will inherit our stdin
            },
            [&](const std::filesystem::path& filepath) {
                stdin_reader = setup_spawn_file_input(filepath);
            },
            [&](stdio_null_t) { stdin_reader = setup_spawn_file_input("/dev/null"); },
        },
        opts.stdin_);

    auto error_io_pipe = create_pipe();

    auto child_pid = ::fork();
    if (child_pid != 0) {
        // We are the parent
        error_io_pipe.writer.close();
        throw_if_error_on_pipe(error_io_pipe.reader);
        imp->pid = child_pid;
        return subprocess{imp.release()};
    }

    // We are the child
    error_io_pipe.reader.close();
    set_cloexec(error_io_pipe.writer.get());

    auto child_fail = [&](std::string_view message) {
        int err = errno;
        error_io_pipe.writer.write(neo::trivial_buffer(err));
        error_io_pipe.writer.write(message);
        std::_Exit(-1);
    };

    // dup2 our stdin
    if (stdin_reader.is_open()) {
        int rc = ::dup2(stdin_reader.get(), STDIN_FILENO);
        if (rc == -1) {
            child_fail("Failed to dup2() for stdin");
        }
    }
    // dup2 our stdout
    if (stdout_writer.is_open()) {
        int rc = ::dup2(stdout_writer.get(), STDOUT_FILENO);
        if (rc == -1) {
            child_fail("Failed to dup2() for stdout");
        }
    }
    // dup2 our stderr
    if (stderr_writer.is_open()) {
        int rc = ::dup2(stderr_writer.get(), STDERR_FILENO);
        if (rc == -1) {
            child_fail("Failed to dup2() for stderr");
        }
    }
    // If they want stderr to go into stdout, set that now
    if (stderr_to_stdout) {
        int rc = ::dup2(STDOUT_FILENO, STDERR_FILENO);
        if (rc == -1) {
            child_fail("Failed to dup2() for redirecting stderr into stdout");
        }
    }

    // Set our working directory
    int rc = ::chdir(workdir.data());
    if (rc == -1) {
        child_fail(chdir_error_message);
    }

    auto exec_fn = opts.env_path_lookup ? ::execvp : ::execv;
    exec_fn(strings[0], (char* const*)strings.data());

    // We should never get to this line if execvp() succeeds
    child_fail(execvp_error_message);
    neo::unreachable();
}

void subprocess::_do_close() noexcept {
    delete _impl;
    _impl = nullptr;
}

void subprocess::_do_join() {
    ::siginfo_t info;
    int         rc = ::waitid(P_PID, _impl->pid, &info, WEXITED);
    if (rc == -1 and errno == EINTR) {
        btr::throw_for_signal();
    }
    neo_assert(invariant,
               rc >= 0,
               "::waitid() failed?",
               rc,
               get_current_error_code().message(),
               _impl->pid,
               info.si_status);

    if (info.si_code == neo::oper::any_of(CLD_KILLED, CLD_DUMPED)) {
        _exit_result = subprocess_exit{.signal_number = info.si_status};
    } else if (info.si_code == CLD_EXITED) {
        _exit_result = subprocess_exit{.exit_code = info.si_status};
    } else {
        neo_assert(invariant,
                   false,
                   "Unexpected waitid() child exit state. This is a bug in vob-batteries.",
                   info.si_status,
                   info.si_errno,
                   info.si_signo,
                   info.si_code);
    }
}

bool subprocess::_do_is_running() const {
    ::siginfo_t info;
    info.si_pid   = 0;
    info.si_signo = 0;
    int rc        = ::waitid(P_PID, _impl->pid, &info, WNOHANG | WEXITED | WNOWAIT);
    if (rc != 0) {
        throw_current_error("Error checking status of child process");
    }
    if (info.si_signo != 0 or info.si_pid != 0) {
        return false;
    } else {
        return true;
    }
}

void subprocess::_do_send_signal(int signum) {
    neo_assert(expects,
               !is_joined(),
               "Attempted to send a signal to a child process that is already joined",
               signum);
    int rc = ::kill(_impl->pid, signum);
    if (rc != 0) {
        throw_current_error("::kill() to send signal to a child process failed");
    }
}

void subprocess::_do_read_output(subprocess_output& out, std::chrono::milliseconds timeout) {
    neo_assert(expects,
               _impl != nullptr,
               "Requested to read output from a detached/moved-from subprocess object");

    ::pollfd poll_fds[3] = {};
    auto     pollfd_out  = poll_fds;

    if (has_stdout()) {
        pollfd_out->fd     = _impl->stdout_pipe.get();
        pollfd_out->events = POLLIN;
        ++pollfd_out;
    }
    if (has_stderr()) {
        pollfd_out->fd     = _impl->stderr_pipe.get();
        pollfd_out->events = POLLIN;
        ++pollfd_out;
    }

    if (pollfd_out == poll_fds) {
        // No file descriptors to poll
        return;
    }

    auto n_fds = static_cast<::nfds_t>(pollfd_out - poll_fds);
    int  rc    = ::poll(poll_fds, n_fds, static_cast<int>(timeout.count()));
    if (rc and errno == EINTR) {
        // We got a signal while waiting. Not an error, but interuption.
        btr::throw_for_signal();
    }
    if (rc == 0) {
        // Timeout!
        return;
    }

    for (::pollfd pfd : neo::ad_hoc_range{poll_fds, pollfd_out}) {
        neo_assert(invariant,
                   pfd.revents & POLLIN or pfd.revents & POLLHUP,
                   "Expected subprocess pipe to be ready for reading",
                   pfd.revents & POLLIN,
                   pfd.revents & POLLOUT,
                   pfd.revents & POLLERR,
                   pfd.revents & POLLHUP);
        bool is_stdout = has_stdout() && (pfd.fd == _impl->stdout_pipe.get());
        if (pfd.revents & POLLHUP and not(pfd.revents & POLLIN)) {
            // Pipe is closed and has no more data for us
            if (is_stdout) {
                _impl->stdout_pipe.close();
            } else {
                _impl->stderr_pipe.close();
            }
            continue;
        }
        std::string& target     = is_stdout ? out.stdout_ : out.stderr_;
        auto         start_size = target.size();
        target.resize(start_size + 1024);
        // Read some data from the pipe
        auto nread = native_io_stream_ref{pfd.fd}.read_into(neo::as_buffer(target) + start_size);
        target.resize(start_size + nread);
        if (nread == 0) {
            // End-of-file
            if (is_stdout) {
                _impl->stdout_pipe.close();
            } else {
                _impl->stderr_pipe.close();
            }
            continue;
        }
    }
}

btr::pipe_reader& subprocess::_do_get_stdout_pipe() const noexcept { return _impl->stdout_pipe; }
btr::pipe_reader& subprocess::_do_get_stderr_pipe() const noexcept { return _impl->stderr_pipe; }
btr::pipe_writer& subprocess::_do_get_stdin_pipe() const noexcept { return _impl->stdin_pipe; }
const subprocess_spawn_options& subprocess::_do_get_spawn_options() const noexcept {
    return _impl->spawn_options;
}

#endif

#pragma once

#include "./pipe.hpp"
#include "./subprocess_fwd.hpp"
#include "./u8view.hpp"

#include <neo/assert.hpp>
#include <neo/opt_ref.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace btr {

class subprocess_failure : public std::runtime_error {
    int _exit_code;
    int _signal_number;

public:
    /**
     * @brief Construct a new subprocess failure exception
     *
     * @param exit_code The exit code of the subprocess, or zero if it exitted with a signal
     * @param signal_number The signal number that caused the subprocess to terminate, or zero if it
     * exited normally
     */
    explicit subprocess_failure(int exit_code, int signal_number) noexcept;

    /**
     * @brief The exit code of the associated process
     */
    int exit_code() const noexcept { return _exit_code; }

    /**
     * @brief The terminating signal number of the associated process
     */
    int signal_number() const noexcept { return _signal_number; }
};

struct subprocess_exit {
    /// The return value of `main()`, or the parameter given to a process 'exit' function.
    int exit_code = 0;
    /// The signal number that caused the process to terminate. Zero if the process exited normally
    int signal_number = 0;

    /// Whether the process exited normally with exit_code of zero
    bool successful() const noexcept { return signal_number == 0 and exit_code == 0; }

    /// If the exit result is not a succcess, throws a @see subprocess_failure
    void throw_if_error() const {
        if (not successful()) {
            throw subprocess_failure(exit_code, signal_number);
        }
    }

    friend void do_repr(auto out, const subprocess_exit* self) noexcept {
        out.type("btr::subprocess_exit");
        if (self) {
            if (self->signal_number != 0) {
                out.bracket_value("signal_number={}", self->signal_number);
            } else if (self->exit_code) {
                out.bracket_value("exit_code={}", self->exit_code);
            } else {
                out.bracket_value("exited zero");
            }
        }
    }
};

struct subprocess_output {
    std::string stdout_;
    std::string stderr_;
};

struct subprocess_result {
    /// The exit result of the subprocess
    subprocess_exit exit;
    /// The output of the subprocess
    subprocess_output output;
};

class subprocess {
public:
    /// Create a communication pipe for the between the current process and the child process
    static inline struct stdio_pipe_t {
    } stdio_pipe;

    /// Allow the child to inherit the associate stdio stream from the parent process (this is the
    /// default)
    static inline struct stdio_inherit_t {
    } stdio_inherit;

    /// Redirect the stderr stream of the child process into its stdout stream.
    static inline struct stderr_to_stdout_t {
    } stderr_to_stdout;

    /// Discard the data from the associated stdio stream. For stdin, immediately gets EOF.
    static inline struct stdio_null_t {
    } stdio_null;

private:
    /// The per-platform implementation of the subprocess
    struct impl;
    impl* _impl;

    /// The exit result, if the subprocess has been joined
    std::optional<subprocess_exit> _exit_result;

    /// Construct a subprocess that adopts the given per-platform impl data
    explicit subprocess(impl* h) noexcept
        : _impl(h) {}

    /// Close the subprocess, if it has not already been detached
    void _destroy() noexcept {
        if (_impl) {
            _terminate_if_unjoined();
            _do_close();
        }
    }

    /// Spawn the subprocess from the given command line, all other options left as default
    template <typename R>
    static subprocess _spawn_cmd(R&& r);

    /// If the subprocess was not joined, terminate the caller
    void _terminate_if_unjoined();

    /// Per-platform impl of join()
    void _do_join();
    /// Per-platform impl of is_running()
    bool _do_is_running() const;
    /// Per-platform impl of send_signal()
    void _do_send_signal(int signum);
    /// Per-platform impl of close()
    void _do_close() noexcept;

    /// Per-platform impl of stdout_pipe()
    btr::pipe_reader& _do_get_stdout_pipe() const noexcept;
    /// Per-platform impl of stderr_pipe()
    btr::pipe_reader& _do_get_stderr_pipe() const noexcept;
    /// Per-platform impl of stdin_pipe()
    btr::pipe_writer& _do_get_stdin_pipe() const noexcept;
    /// Per-platform impl of spawn_options()
    const subprocess_spawn_options& _do_get_spawn_options() const noexcept;

    /// Per-platform impl of read_output()
    void _do_read_output(subprocess_output& out, std::chrono::milliseconds timeout);

    void _repr_into(std::string& out) const noexcept;

public:
    /// Cleanup
    ~subprocess() { _destroy(); }

    /// Move construct
    subprocess(subprocess&& o) noexcept { *this = std::move(o); }

    /// Move-assign
    subprocess& operator=(subprocess&& o) noexcept {
        _destroy();
        _impl        = std::exchange(o._impl, nullptr);
        _exit_result = std::exchange(o._exit_result, std::nullopt);
        return *this;
    }

    /**
     * @brief Spawn a new subprocess and return a handle to that process
     *
     * @param opts The startup parameters for the subprocess
     *
     * @return subprocess The executing subprocess
     */
    [[nodiscard]] static subprocess spawn(subprocess_spawn_options const& opts);

    /**
     * @brief Spawn a new subprocess that executes the given command
     *
     * @param cmd A sequence of string_view-convertible command arguments.
     *
     * @return subprocess The executing subprocess
     */
    template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, std::string_view>  //
        [[nodiscard]] static subprocess spawn(R&& cmd) {
        return _spawn_cmd(NEO_FWD(cmd));
    }

    [[nodiscard]] static subprocess spawn(std::initializer_list<std::string_view> cmd) {
        return _spawn_cmd(cmd);
    }

    template <std::convertible_to<std::string_view> S>
    [[nodiscard]] static subprocess spawn(std::initializer_list<S> il) {
        return _spawn_cmd(il);
    }

    /**
     * @brief Read output from the subprocess into the accumulation result 'out'
     *
     * @param out The stdout/stderr accumulation output parameter
     * @param timeout The read timeout. If -1, waits forever. If zero, returns immediately
     */
    void read_output_into(subprocess_output& out, std::chrono::milliseconds timeout) {
        _do_read_output(out, timeout);
    }

    /**
     * @brief Read output from the subprocess into the accumulation result. Blocks
     * until any amount of data has been read for either stdout or stderr.
     *
     * @param out The stdout/stderr accumulating output parameter
     */
    void read_output_into(subprocess_output& out) {
        return read_output_into(out, std::chrono::milliseconds{-1});
    }

    /**
     * @brief Read the entirety of stdout and stderr from the subprocess. Blocks
     * until both stdout and stderr pipes have been closed by the subprocess.
     *
     * @return subprocess_output The complete output of the subprocess
     */
    [[nodiscard]] subprocess_output read_output();

    /**
     * @brief Write some data into the stdin pipe of the subprocess. Expects has_stdin() to be true
     *
     * @param str The string to write to the process
     * @return std::size_t The number of bytes that were written.
     */
    std::size_t write_input(trivial_range auto&& buf) {
        neo_assertion_breadcrumbs("Writing data into subprocess stdin");
        return stdin_pipe().write(buf);
    }

    /**
     * @brief Close the stdin writing handle to the subprocess.
     */
    void close_stdin() noexcept;

    /**
     * @brief Obtain a reference to the underlying stdout pipe, if it is open.
     */
    [[nodiscard]] btr::pipe_reader&       stdout_pipe() noexcept { return _do_get_stdout_pipe(); }
    [[nodiscard]] const btr::pipe_reader& stdout_pipe() const noexcept {
        return _do_get_stdout_pipe();
    }

    /**
     * @brief Obtain a reference to the underlying stderr pipe, if it is still open.
     */
    [[nodiscard]] btr::pipe_reader&       stderr_pipe() noexcept { return _do_get_stderr_pipe(); }
    [[nodiscard]] const btr::pipe_reader& stderr_pipe() const noexcept {
        return _do_get_stderr_pipe();
    }

    /**
     * @brief Obtain a reference to the underlying stdin pipe, if it is still open.
     */
    [[nodiscard]] btr::pipe_writer&       stdin_pipe() noexcept { return _do_get_stdin_pipe(); }
    [[nodiscard]] const btr::pipe_writer& stdin_pipe() const noexcept {
        return _do_get_stdin_pipe();
    }

    /// Determine whether stdout is (still) open
    [[nodiscard]] bool has_stdout() const noexcept { return stdout_pipe().is_open(); }
    /// Determine whether stderr is (still) open
    [[nodiscard]] bool has_stderr() const noexcept { return stderr_pipe().is_open(); }
    /// Determine whether stdin is (still) open
    [[nodiscard]] bool has_stdin() const noexcept { return stdin_pipe().is_open(); }
    /// Obtain the options that were used to spawn the subprocess
    [[nodiscard]] const subprocess_spawn_options& spawn_options() const noexcept {
        return _do_get_spawn_options();
    }

    /// Check whether join() has been called
    [[nodiscard]] bool is_joined() const noexcept { return _exit_result.has_value(); }
    /// Check if the subprocess is still running.
    [[nodiscard]] bool is_running() const noexcept { return !is_joined() and _do_is_running(); }
    /// Reap and join the subprocess, and set and return the exit result.
    const subprocess_exit& join();
    /// Detach from the subprocess. Closes all open pipes and handles.
    void detach() noexcept;

    /// Send an OS signal to the subprocess.
    void send_signal(int signum) { _do_send_signal(signum); }

    /**
     * @brief Attempt to join() the running subprocess without waiting.
     *
     * @returns The value of exit_result(). Will be nullopt if the process did not join().
     */
    const std::optional<subprocess_exit>& try_join() {
        if (!is_running()) {
            join();
        }
        return exit_result();
    }

    /**
     * @brief Obtain the subprocess_exit result of this process. If the process
     * has not been join()'d, then returns nullopt
     */
    const std::optional<subprocess_exit>& exit_result() const noexcept { return _exit_result; }

    friend void do_repr(auto out, const subprocess* self) noexcept {
        out.type("btr::subprocess");
        if (self) {
            self->_repr_into(out.underlying_string());
        }
    }
};

struct subprocess_spawn_options {
private:
    static auto _repr_pipe_opt_1(auto out, const std::filesystem::path& p) {
        return "[into file: " + out.repr_value(p).string() + "]";
    }

    static auto _repr_pipe_opt_1(auto, subprocess::stderr_to_stdout_t) {
        return "[redirect to stdout]";
    }
    static auto _repr_pipe_opt_1(auto, subprocess::stdio_inherit_t) { return "[inherit]"; }
    static auto _repr_pipe_opt_1(auto, subprocess::stdio_null_t) { return "[to-null]"; }
    static auto _repr_pipe_opt_1(auto, subprocess::stdio_pipe_t) { return "[piped]"; }

    static std::string _repr_pipe_opt(auto out, auto const& opt) {
        return std::visit([&](const auto& el) -> std::string { return _repr_pipe_opt_1(out, el); },
                          opt);
    }

public:
    /**
     * @brief The command to execute.
     *
     * The argument strings will be passed to the subprocess's main() function
     * via the pointer-to-array second argument to main().
     *
     * If the 'program' option is not specified, then the first argument is used
     * as the name/path of an executable to run.
     *
     * If any of the arguments contain embedded nulls, that argument will be truncated when passed
     * to the subprocess.
     */
    std::vector<std::string> command;

    /**
     * @brief The program to execute, or nullopt.
     *
     * If not provided, the program to execute is taken from the first string
     * in 'command'. Either 'command' must be non-empty or 'program' must be specified.
     */
    std::optional<std::filesystem::path> program{};

    /**
     * @brief The working directory of the new subprocess, or nullopt.
     *
     * If not provided, the child process will inherit the working directory of
     * the caller.
     */
    std::optional<std::filesystem::path> working_directory{};

    /**
     * @brief Control the new stdin stdio stream for the spawned subproces.
     *
     * If `subprocess::stdio_pipe`, then a pipe is opened to the subprocess, and
     * the caller may send data to the stdin of the subprocess
     *
     * If `subprocess::stdio_inherit`, then the subprocess inherits
     * the stdin pipe from the parent.
     *
     * If given a `std::filesystem::path`, then that file is opened and used as the
     * destination of the stderr for the child proces.
     *
     * If given `subprocess::stdio_null` (the default), then the subprocess will immediately
     * hit EOF when it reads from stdin.
     *
     * If given `subprocess::stderr_to_stdout`, then stderr of the subprocess
     * is redirected into the stdout of that subprocess, essentially sharing
     * the stream.
     */
    std::variant<subprocess::stdio_null_t,
                 subprocess::stdio_inherit_t,
                 subprocess::stdio_pipe_t,
                 std::filesystem::path>
        stdin_{};

    /**
     * @brief Control the new stdout stdio stream for the spawned subproces.
     *
     * If `subprocess::stdio_pipe`, then a pipe is opened to the subprocess from
     * which the caller can read the output from the child.
     *
     * If `subprocess::stdio_inherit` (the default), then the subprocess inherits
     * the stdout pipe from the parent.
     *
     * If given a `std::filesystem::path`, then that file is opened and used as the
     * destination of the stdout for the child proces.
     *
     * If given `subprocess::stdio_null`, then stdout data from the process will
     * be discarded.
     */
    std::variant<subprocess::stdio_inherit_t,
                 subprocess::stdio_pipe_t,
                 subprocess::stdio_null_t,
                 std::filesystem::path>
        stdout_{};

    /**
     * @brief Control the new stderr stdio stream for the spawned subproces.
     *
     * If `subprocess::stdio_pipe`, then a pipe is opened to the subprocess from
     * which the caller can read the error output from the child.
     *
     * If `subprocess::stdio_inherit` (the default), then the subprocess inherits
     * the stderr pipe from the parent.
     *
     * If given a `std::filesystem::path`, then that file is opened and used as the
     * destination of the stderr for the child proces.
     *
     * If given `subprocess::stdio_null`, then stderr data from the process will
     * be discarded.
     *
     * If given `subprocess::stderr_to_stdout`, then stderr of the subprocess
     * is redirected into the stdout of that subprocess, essentially sharing
     * the stream.
     */
    std::variant<subprocess::stdio_inherit_t,
                 subprocess::stdio_pipe_t,
                 subprocess::stdio_null_t,
                 subprocess::stderr_to_stdout_t,
                 std::filesystem::path>
        stderr_{};

    /**
     * @brief Whether the executable should be looked up on the PATH environment
     * variable. Default is 'true'. If 'false', then the program should be an
     * absolute or relative path relative to the workging_directory of the subprocess.
     */
    bool env_path_lookup = true;

    /**
     * @brief If 'true', the new process will be made its own group leader.
     *
     * @note This has the effect that it will not share signal delivery with the
     * parent's process group. It is then up to the caller to forward signals
     * to the child process.
     */
    bool set_group_leader = false;

    friend void do_repr(auto out, const subprocess_spawn_options* self) noexcept {
        out.type("btr::subprocess_spawn_options");
        if (self) {
            out.append("{command={}", out.repr_value(self->command));
            if (self->program) {
                out.append(", program={}", out.repr_value(self->program));
            }
            if (self->working_directory) {
                out.append(", working_direcotory={}", out.repr_value(self->working_directory));
            }
            out.append(", stdin={}, stdout={}, stderr={}",
                       _repr_pipe_opt(out, self->stdin_),
                       _repr_pipe_opt(out, self->stdout_),
                       _repr_pipe_opt(out, self->stderr_));
            if (self->set_group_leader) {
                out.append(", set-group-leader");
            }
            if (self->env_path_lookup && !self->program) {
                out.append(", env-path-lookup");
            }
            out.append("}");
        }
    }
};

template <typename R>
subprocess subprocess::_spawn_cmd(R&& r) {
    subprocess_spawn_options opts;
    for (std::string_view arg : r) {
        opts.command.emplace_back(arg);
    }
    return spawn(opts);
}

bool        argv_arg_needs_quoting(u8view arg) noexcept;
std::string quote_argv_arg(u8view arg) noexcept;

template <std::ranges::input_range R>
requires std::convertible_to<std::ranges::range_value_t<R>, u8view>  //
    std::string quote_argv_string(R&& r) noexcept {
    std::string acc;
    for (u8view arg : r) {
        acc.append(quote_argv_arg(arg));
        acc.push_back(' ');
    }
    if (!acc.empty()) {
        acc.pop_back();
    }
    return acc;
}

}  // namespace btr

#pragma once

#include <csignal>
#include <stdexcept>

namespace btr {

/**
 * @brief Notify that the given signal has been received. This is intended to be called as/from a
 * signal handler
 *
 * @param signum The number of the signal that was received
 */
void notify_received_signal(int signum) noexcept;

/**
 * @brief Clear the received signal number
 */
inline void reset_signal() noexcept { btr::notify_received_signal(0); }

/**
 * @brief Obtain the number of the most recently received signal. Returns zero if not signal number
 * is notified.
 */
[[nodiscard]] int received_signal() noexcept;

/**
 * @brief A scoped signal handler.
 *
 * When constructed, the given signal handler will be tied to the associated
 * signal number. Upon destruction, the prior signal handler will be restored.
 */
class signal_handling_scope {
    int _signum;
    void (*_prev_action)(int);

public:
    /// Associate the @param action signal handler with @param signum
    [[nodiscard]] signal_handling_scope(int signum, void (*action)(int)) noexcept
        : _signum{signum}
        , _prev_action{std::signal(signum, action)} {}

    /// Create a scoped handler for @param signum that calls `notify_received_signal`
    [[nodiscard]] explicit signal_handling_scope(int signum) noexcept
        : signal_handling_scope(signum, notify_received_signal) {}

    // Restore the prior signal handler
    ~signal_handling_scope() { std::signal(_signum, _prev_action); }

    // This type is immobile
    signal_handling_scope(signal_handling_scope&&) = delete;
    signal_handling_scope& operator=(signal_handling_scope&&) = delete;
};

/**
 * @brief Sets up several signal handlers to handle common user program termination requests.
 *
 * The following are handled: SIGTERM, SIGINT, SIGQUIT, SIGHUP, and SIGBREAK
 *
 * The notify_received_signal function will be called upon receipt of a signal.
 * It is up to the application to respect that a signal was delivered.
 */
class default_signal_handling_scope {
    using _sig_action_t = void (*)(int);

#ifdef SIGTERM
    signal_handling_scope _sigterm{SIGTERM};
#endif
#ifdef SIGINT
    signal_handling_scope _sigint{SIGINT};
#endif
#ifdef SIGQUIT
    signal_handling_scope _sigquit{SIGQUIT};
#endif
#ifdef SIGHUP
    signal_handling_scope _sighup{SIGHUP};
#endif
#ifdef SIGBREAK
    signal_handling_scope _sigbreak{SIGBREAK};
#endif

public:
    [[nodiscard]] default_signal_handling_scope() = default;
};

/**
 * @brief Throw an exception corresponding to the given signal number
 *
 * @param signum
 */
[[noreturn]] void throw_for_signal(int signum);

/**
 * @brief Throw an exception corresponding to the most recently received signal
 */
[[noreturn]] void throw_for_signal();

/**
 * @brief If a signal was received, throw an exception for that signal
 *
 */
void throw_if_signalled();

/**
 * @brief Exception class used to encapsulate the delivery of a signal to the current process.
 *
 * This class cannot be constructed directly. Use the 'btr::throw_for_signal' function to
 * throw an exception of this type.
 */
class signal_exception : public std::exception {
    int _signal_number;

protected:
    explicit signal_exception(int i) noexcept
        : _signal_number{i} {}

    friend void btr::throw_for_signal(int);

public:
    const char* what() const noexcept override {
        return "The operation was interrupted by a signal delivered to the current process";
    }

    /**
     * @brief Obtain the integral signal number associated with this exception
     */
    int signal_number() const noexcept { return _signal_number; }
};

/**
 * @brief Base class of all signal exceptions that correspond to user-initiated termination
 * requests. Cannot be constructed directly.
 */
struct terminating_signal_exception : signal_exception {
    using signal_exception::signal_exception;
};

/**
 * @brief Placeholder class for signal exceptions that correspond to a signal number
 * that is not available on the current platform. This class cannot be constructed.
 */
struct no_such_signal_on_this_platform : signal_exception {
    no_such_signal_on_this_platform() = delete;
};

/**
 * @brief Signal exception class representing an interactive interrupt request (i.e. pressing Ctrl+C
 * in the process's terminal or console window).
 */
#ifdef SIGINT
struct sigint_exception : terminating_signal_exception {
    sigint_exception() noexcept
        : terminating_signal_exception{SIGINT} {}
};
#else
using sigint_exception   = no_such_signal_on_this_platform;
#endif

/**
 * @brief Signal exception class representing a interrupt request from another process on the
 * system.
 *
 * @note Not available on Windows.
 */
#ifdef SIGTERM
struct sigterm_exception : terminating_signal_exception {
    sigterm_exception() noexcept
        : terminating_signal_exception{SIGTERM} {}
};
#else
using sigterm_exception  = no_such_signal_on_this_platform;
#endif

/**
 * @brief Signal exception class representing a request that the applicatoin quit.
 *
 * @note Not available on Windows
 */
#ifdef SIGQUIT
struct sigquit_exception : terminating_signal_exception {
    sigquit_exception() noexcept
        : terminating_signal_exception{SIGQUIT} {}
};
#else
using sigquit_exception  = no_such_signal_on_this_platform;
#endif

/**
 * @brief Signal exception class representing a hang-up from the controlling terminal.
 *
 * @note Not available on Windows
 */
#ifdef SIGHUP
struct sighup_exception : terminating_signal_exception {
    sighup_exception() noexcept
        : terminating_signal_exception{SIGHUP} {}
};
#else
using sighup_exception   = no_such_signal_on_this_platform;
#endif

/**
 * @brief Signal exception class corresponding to Window's SIGBREAK
 *
 * @note Only available on Windows
 */
#ifdef SIGBREAK
struct sigbreak_exception : terminating_signal_exception {
    sigbreak_exception() noexcept
        : terminating_signal_exception{SIGBREAK} {}
};
#else
using sigbreak_exception = no_such_signal_on_this_platform;
#endif

}  // namespace btr

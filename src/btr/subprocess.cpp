#include "./subprocess.hpp"

#include <neo/assert.hpp>

#include <span>

using namespace btr;

void subprocess::_terminate_if_unjoined() {
    neo_assert_always(expects,
                      not _impl or is_joined(),
                      "btr::subprocess was destroyed, but was never joined nor detached.", );
}

const subprocess_exit& subprocess::join() {
    neo_assert(expects,
               _impl != nullptr,
               "subprocess::join() was called on a null/detached subprocess");

    neo_assert(expects,
               !is_joined(),
               "subprocess::join() was called on an already-joined subprocess",
               exit_result()->exit_code,
               exit_result()->signal_number);

    _do_join();
    neo_assert(invariant,
               exit_result().has_value(),
               "subprocess::_do_join() did not set _exit_result as required");
    return *exit_result();
}

void subprocess::detach() noexcept { _do_close(); }

subprocess_output subprocess::read_output() {
    subprocess_output ret;
    while (has_stdout() or has_stderr()) {
        read_output_into(ret);
    }
    return ret;
}

void subprocess::close_stdin() noexcept {
    neo_assertion_breadcrumbs("Closing stdin of a subprocess");
    stdin_pipe().close();
}

subprocess_failure::subprocess_failure(int exit_code, int signo) noexcept
    : runtime_error(signo ? neo::ufmt("Subprocess was terminated by signal {}", signo)
                          : neo::ufmt("Subprocess exited [{}]", exit_code))
    , _exit_code(exit_code)
    , _signal_number(signo) {}

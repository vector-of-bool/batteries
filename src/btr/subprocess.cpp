#include "./subprocess.hpp"

#include "./utf.hpp"

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

bool btr::argv_arg_needs_quoting(u8view arg) noexcept {
    auto                sv = arg.string_view();
    codepoint_range     cps{sv};
    std::u32string_view okay_chars = U"@%-+=:,./|_";
    const bool          all_okay   = std::all_of(cps.begin(), cps.end(), [&](char32_t c) {
        return std::isalnum(c) || (okay_chars.find(c) != okay_chars.npos);
    });
    return !all_okay;
}

std::string btr::quote_argv_arg(u8view arg) noexcept {
    if (!argv_arg_needs_quoting(arg)) {
        return std::string(arg);
    }
    codepoint_range cps{arg.u8string_view()};
    std::u32string  r;
    for (char32_t c : cps) {
        if (c == '\\') {
            r.append(U"\\\\");
        } else if (c == '"') {
            r.append(U"\\\"");
        } else {
            r.push_back(c);
        }
    }
    return u8_as_char_encode(r);
}

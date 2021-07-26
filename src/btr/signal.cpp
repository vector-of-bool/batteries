#include "./signal.hpp"

using namespace btr;

namespace {

std::sig_atomic_t S_received_signal = 0;

}  // namespace

void btr::notify_received_signal(int signum) noexcept {
    S_received_signal = static_cast<std::sig_atomic_t>(signum);
}

int btr::received_signal() noexcept { return S_received_signal; }

void btr::throw_for_signal() { throw_for_signal(received_signal()); }

void btr::throw_if_signalled() {
    if (received_signal() != 0) {
        throw_for_signal(received_signal());
    }
}

void btr::throw_for_signal(int signum) {
    switch (signum) {
#ifdef SIGINT
    case SIGINT:
        throw sigint_exception();
#endif
#ifdef SIGTERM
    case SIGTERM:
        throw sigterm_exception();
#endif
#ifdef SIGQUIT
    case SIGQUIT:
        throw sigquit_exception();
#endif
#ifdef SIGBREAK
    case SIGBREAK:
        throw sigbreak_exception();
#endif
#ifdef SIGHUP
    case SIGHUP:
        throw sighup_exception();
#endif
    default:
        throw signal_exception(signum);
    }
}

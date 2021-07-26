#include "./syserror.hpp"

#if !_WIN32

#include <cerrno>

using namespace btr;

int  btr::get_current_error() noexcept { return errno; }
void btr::set_current_error(int e) noexcept { errno = e; }
void btr::clear_current_error() noexcept { set_current_error(0); }

#endif

#include "./syserror.hpp"

#if _WIN32

#include <windows.h>

using namespace btr;

int  btr::get_current_error() noexcept { return static_cast<int>(::GetLastError()); }
void btr::set_current_error(int e) noexcept { ::SetLastError(static_cast<DWORD>(e)); }
void btr::clear_current_error() noexcept { set_current_error(0); }

#endif

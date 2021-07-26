#include "./syserror.hpp"

#include <system_error>

using namespace btr;

void btr::throw_for_system_error_code(int error, u8view message) {
    throw std::system_error(std::error_code(error, std::system_category()), std::string(message));
}

void btr::throw_current_error(u8view message) {
    throw_for_system_error_code(get_current_error(), message);
}

void btr::throw_if_current_error(u8view message) {
    if (int ec = get_current_error()) {
        throw_for_system_error_code(ec, message);
    }
}

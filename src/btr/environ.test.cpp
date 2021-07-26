#include "./environ.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Get an environment variable") {
    auto path = btr::getenv("PATH");
    CHECK(path);

    auto u8path = btr::u8getenv("PATH");
    CHECK(*path == btr::u8view(*u8path).string_view());
}

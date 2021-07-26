#include "./utf.hpp"

#include <catch2/catch.hpp>

using namespace std::literals;

TEST_CASE("Encode a simple string") {
    std::u8string s = btr::transcode_string<char8_t>("Hello!"sv);
    CHECK(s == u8"Hello!");

    s = btr::transcode_string<char8_t>(L"This is a euro symbol: €"sv);
    CHECK(s == u8"This is a euro symbol: €");

    auto s2 = btr::transcode_string<char>("€42"sv);
    CHECK(s2 == "€42");
}

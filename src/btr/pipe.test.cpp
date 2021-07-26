#include "./pipe.hpp"

#include <catch2/catch.hpp>

TEST_CASE("Create a pipe") {
    auto p = btr::create_pipe();
    p.writer.write("I am a string");
    auto b = p.reader.read(388);
    CHECK(b == "I am a string");

    p.writer.write("foobar");
    b = p.reader.read(3);
    CHECK(b == "foo");
    b = p.reader.read(3);
    CHECK(b == "bar");
}

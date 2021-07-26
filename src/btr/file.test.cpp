#include "./file.hpp"

#include <catch2/catch.hpp>

#include <fstream>

auto THIS_DIR = std::filesystem::weakly_canonical(std::filesystem::path(__FILE__).parent_path());

TEST_CASE("Open this file") {
    auto content = btr::file::read(__FILE__);
    auto found   = content.find("Find this string");
    CHECK(found != content.npos);
}

TEST_CASE("Open a non-existent file") {
    CHECK_THROWS_AS(btr::file::open(THIS_DIR / "non-existent-subdir/file.txt"),
                    btr::file_not_found_error);

    CHECK_THROWS_AS(btr::file::open(THIS_DIR / "file-does-not-exist.txt"),
                    btr::file_not_found_error);
}

TEST_CASE("Read and write a file") {
    auto fpath = THIS_DIR / "test-write-read.txt";
    btr::file::write(fpath, "I am a string!\n");
    auto content = btr::file::read(fpath);
    CHECK(content == "I am a string!\n");
    std::filesystem::remove(fpath);
}

TEST_CASE("Write some non-byte-sized data") {
    std::u16string str = u"I am a string";
    btr::file::write(THIS_DIR / "test.data", str);
    str.clear();
    str.resize(64);
    auto f = btr::file::open(THIS_DIR / "test.data");
    str.resize(f.read_into(str));
    CHECK(str == u"I am a string");
    std::filesystem::remove(THIS_DIR / "test.data");
}

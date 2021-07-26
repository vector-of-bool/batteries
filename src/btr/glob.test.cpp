#include "./glob.hpp"

#include <catch2/catch.hpp>

const auto THIS_DIR = std::filesystem::path(__FILE__).parent_path();

TEST_CASE("Create a glob") { auto glob = btr::glob::compile("*.test.cpp"); }

TEST_CASE("Scan a directory") {
    auto root_dir = std::filesystem::weakly_canonical(THIS_DIR / "../..").lexically_normal();
    auto data_dir = root_dir / "data";

    auto found = btr::glob::compile("glob-test-1/**/foo/**/glob-test-2/**/*.txt")
                     .search(data_dir)
                     .to_vector();
    CHECK(found.size() == 2);

    found = btr::glob::compile("glob-test-1/foo/*/glob-test-2/bar/**/*.txt")
                .search(data_dir)
                .to_vector();
    CHECK(found.size() == 2);
}

TEST_CASE("Check globs") {
    auto glob = btr::glob::compile("foo/bar*/baz");
    CHECK(glob.test("foo/bar/baz"));
    CHECK(glob.test("foo/barffff/baz"));
    CHECK_FALSE(glob.test("foo/bar"));
    CHECK_FALSE(glob.test("foo/ffbar/baz"));
    CHECK_FALSE(glob.test("foo/bar/bazf"));
    CHECK_FALSE(glob.test("foo/bar/"));

    glob = btr::glob::compile("foo/**/bar.txt");
    CHECK(glob.test("foo/bar.txt"));
    CHECK(glob.test("foo/thing/bar.txt"));
    CHECK(glob.test("foo/thing/another/bar.txt"));
    CHECK_FALSE(glob.test("foo/fail"));
    CHECK_FALSE(glob.test("foo/bar.txtf"));
    CHECK_FALSE(glob.test("foo/bar.txt/f"));
    CHECK_FALSE(glob.test("foo/fbar.txt"));
    CHECK_FALSE(glob.test("foo/thing/fail"));
    CHECK_FALSE(glob.test("foo/thing/another/fail"));
    CHECK_FALSE(glob.test("foo/thing/bar.txt/fail"));
    CHECK_FALSE(glob.test("foo/bar.txt/fail"));

    glob = btr::glob::compile("foo/**/bar/**/baz.txt");
    CHECK(glob.test("foo/bar/baz.txt"));
    CHECK(glob.test("foo/thing/bar/baz.txt"));
    CHECK(glob.test("foo/thing/bar/baz.txt"));
    CHECK(glob.test("foo/thing/bar/thing/baz.txt"));
    CHECK(glob.test("foo/bar/thing/baz.txt"));
    CHECK(glob.test("foo/bar/baz/baz.txt"));

    glob = btr::glob::compile("doc/**");
    CHECK(glob.test("doc/something.txt"));
}

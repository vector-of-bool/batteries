#include <btr/fnmatch.hpp>

#include <catch2/catch.hpp>

TEST_CASE("Basic fnmatch matching") {
    auto pat = btr::fnmatch_pattern::compile("foo.bar");
    CHECK_FALSE(pat.test("foo.baz"));
    CHECK_FALSE(pat.test("foo."));
    CHECK_FALSE(pat.test("foo.barz"));
    CHECK_FALSE(pat.test("foo.bar "));
    CHECK_FALSE(pat.test(" foo.bar"));
    CHECK(pat.test("foo.bar"));

    pat = btr::fnmatch_pattern::compile("foo.*");
    CHECK(pat.test("foo."));
    auto m = pat.test("foo.b");
    CHECK(m);
    CHECK(pat.test("foo. "));
    CHECK_FALSE(pat.test("foo"));
    CHECK_FALSE(pat.test(" foo.bar"));

    pat = btr::fnmatch_pattern::compile("foo.*.cpp");
    for (auto fname : {"foo.bar.cpp", "foo..cpp", "foo.cat.cpp"}) {
        auto m = pat.test(fname);
        CHECK(m);
    }

    for (auto fname : {"foo.cpp", "foo.cpp"}) {
        auto m = pat.test(fname);
        CHECK_FALSE(m);
    }
}

TEST_CASE("Cases") {
    struct case_ {
        bool             match;
        std::string_view pattern;
        std::string_view given;
    };

    auto [expect_match, pat_str, test_str] = GENERATE(Catch::Generators::values<case_>({
        {true, "foo", "foo"},              // Basic literal pattern
        {true, "", ""},                    // Empty pattern
        {false, "", "f"},                  // Empty should be empty
        {true, "?", "f"},                  // Single-char is okay
        {false, "?", "ff"},                // Two is not
        {true, "??", "ff"},                // unless there are two
        {false, "[abc]", "A"},             // Group is okay
        {true, "[abc]", "a"},              // Group is okay
        {true, "[!abc]", "A"},             // Negative grouup is okay
        {false, "[!abc]", "a"},            // Negative grouup is okay
        {true, "[abc][123]", "a1"},        // Two groups
        {true, "[abc]*[123]", "a1"},       // Two groups
        {true, "[abc]def[123]", "adef1"},  // Two groups
        {true, "*foo", "foo"},             // Suffix pattern
        {true, "*foo", "barfoo"},          // Suffix pattern
        {false, "*foo", "fooo"},           // Suffix pattern
        {true, "bar*foo", "barfoo"},       // Affix pattern
        {true, "bar*", "barfoo"},          // Prefix pattern
        {true, "bar*?", "barfoo"},         // Mixed

        {true, "Кириллица", "Кириллица"},           // Unicode
        {false, "Кириллица*foo", "Кириллица"},      // Unicode
        {true, "Кириллица*foo", "Кириллицаfoo"},    // Unicode
        {true, "Кириллица*foo", "Кириллица--foo"},  // Unicode
        {true, "Кири[лabc]лица", "Кириллица"},      // Unicode
        {true, "Кири[лabc]лица", "Кириaлица"},      // Unicode
        {false, "Кири[!л]лица", "Кириллица"},       // Unicode
        {true, "Кири[!л]лица", "Кириqлица"},        // Unicode

        {true, "[?]", "?"},    // Escape via grouping
        {true, "[?]?", "?f"},  // Escape via grouping
        {false, "[?]", "f"},   // Escape via grouping
        {true, "[!]", "!"},    // Escape via grouping
        {false, "[!]", "f"},   // Escape via grouping
        {false, "[!!]", "!"},  // Escape via grouping
        {true, "[!!]", "f"},   // Escape via grouping
        {true, "[]]", "]"},    // Escape via grouping
        {false, "[]]", "f"},   // Escape via grouping
        {true, "[[]", "["},    // Escape via grouping
        {false, "[[]", "]"},   // Escape via grouping
        {true, "[![]", "f"},   // Escape via grouping
        {false, "[![]", "["},  // Escape via grouping

        {true, "*************", "a"},  // Pathological
        {true,
         "*a*a*a*a*a*a*a*a*aba*a*a*a*a*a*a*a*a*a*a*a*a*a*ab",
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaazaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
         "aaaaaaaaaab"},  // Pathological
    }));

    CAPTURE(pat_str, test_str, expect_match);
    const auto pat = btr::fnmatch_pattern::compile(pat_str);
    CHECK(pat.literal_spelling() == pat_str);
    const bool did_match = pat.test(test_str);
    CHECK(did_match == expect_match);
}

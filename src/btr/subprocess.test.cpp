#include "./subprocess.hpp"

#include <btr/file.hpp>
#include <btr/signal.hpp>

#include <catch2/catch.hpp>

#include <neo/platform.hpp>

TEST_CASE("Spawn a simple processs") {
    if (neo::os_is_unix_like) {
        auto proc = btr::subprocess::spawn({"/bin/sh", "-c", "echo hello"});
        auto rc   = proc.join();
        CHECK(rc.exit_code == 0);

        proc = btr::subprocess::spawn({
            .command = {"/bin/bash", "-c", "echo hello; cat > /dev/null; exit 42"},
            .stdin_  = btr::subprocess::stdio_pipe,
            .stdout_ = std::filesystem::path("output.txt"),
        });
        REQUIRE(proc.is_running());
        REQUIRE_FALSE(proc.is_joined());
        REQUIRE_FALSE(proc.try_join());
        proc.close_stdin();
        while (proc.is_running()) {
            // spin
        }
        CHECK_FALSE(proc.is_running());
        proc.send_signal(SIGINT);
        proc.join();
        CHECK_FALSE(proc.is_running());
        CHECK(proc.is_joined());
        CHECK(proc.exit_result().has_value());
        CHECK(proc.exit_result()->exit_code == 42);

        auto content = btr::file::read("output.txt");
        std::filesystem::remove("output.txt");
        CHECK(content == "hello\n");

        proc = btr::subprocess::spawn(
            {.command = {"/bin/sh", "-c", "echo hello"}, .stdout_ = btr::subprocess::stdio_pipe});
        btr::subprocess_output out;
        proc.join();
        CHECK(proc.has_stdout());
        proc.read_output_into(out);
        proc.read_output_into(out);
        CHECK_FALSE(proc.has_stdout());
        CHECK(out.stdout_ == "hello\n");
        proc.read_output_into(out);

        proc = btr::subprocess::spawn({
            .command = {"/bin/sh", "-c", "echo Howdy"},
            .stdout_ = btr::subprocess::stdio_pipe,
        });
        out  = proc.read_output();
        CHECK(out.stdout_ == "Howdy\n");
        CHECK(out.stderr_ == "");
        CHECK(proc.join().exit_code == 0);

        proc = btr::subprocess::spawn({"/bin/sh", "-c", "sleep 10"});
        proc.send_signal(SIGINT);
        proc.join();
        CHECK(proc.exit_result()->signal_number == SIGINT);

        auto this_dir    = std::filesystem::current_path();
        auto test_subdir = this_dir / "_test";
        std::filesystem::create_directory(test_subdir);
        proc = btr::subprocess::spawn({
            .command           = {"/bin/sh", "-c", "echo 'hello' > output.txt"},
            .working_directory = test_subdir,
        });
        proc.join();
        content = btr::file::read(test_subdir / "output.txt");
        CHECK(content == "hello\n");

        try {
            btr::subprocess::spawn({"this-exe-does-not-exist.exe"}).join();
            FAIL_CHECK("No-executable test did not fail");
        } catch (const std::system_error& e) {
            CHECK(e.code() == std::errc::no_such_file_or_directory);
        }

        try {
            btr::subprocess::spawn({"/bin/sh", "-c", "exit 42"}).join().throw_if_error();
            FAIL_CHECK("No exception was thrown");
        } catch (const btr::subprocess_failure& e) {
            CHECK(e.exit_code() == 42);
            CHECK(e.signal_number() == 0);
        }

        btr::subprocess::spawn(  //
            {
                .command = {"/bin/sh", "-c", "echo hello"},
                .stdout_ = btr::subprocess::stdio_null,
            })
            .join()
            .throw_if_error();

        proc = btr::subprocess::spawn(  //
            {
                .command = {"/bin/cat"},
                .stdin_  = std::filesystem::path(__FILE__),
                .stdout_ = btr::subprocess::stdio_pipe,
            });
        proc.join().throw_if_error();
        auto output = proc.read_output();
        CHECK(output.stdout_ == btr::file::read(__FILE__));

        proc = btr::subprocess::spawn({
            .command = {"/bin/cat"},
            .stdin_  = btr::subprocess::stdio_pipe,
            .stdout_ = btr::subprocess::stdio_pipe,
        });
        REQUIRE(proc.has_stdin());
        proc.write_input("Hello!");
        proc.close_stdin();
        proc.join();
        output = proc.read_output();
        CHECK(output.stdout_ == "Hello!");
    }
}

#include <catch2/catch_test_macros.hpp>
#include "snip/fallback_engine.hpp"
#include "snip/dispatcher.hpp"

TEST_CASE("FallbackEngine strips ANSI escape and OSC hyperlink sequences", "[fallback]") {
    std::string raw = "\033[31m[ERROR]\033[0m \033]8;;https://example.com/docs\007Documentation\033]8;;\007 failed to compile.\n";
    auto res = snip::FallbackEngine::process({"my_custom_tool"}, raw, 1);

    REQUIRE(res.text.find("\033[") == std::string::npos);
    REQUIRE(res.text.find("\033]") == std::string::npos);
    REQUIRE(res.text.find("[ERROR] Documentation failed to compile.") != std::string::npos);
}

TEST_CASE("FallbackEngine rewinds carriage return progress bars", "[fallback]") {
    std::string raw = "Progress: 10%\rProgress: 50%\rProgress: 100%\nCompilation finished.\n";
    auto res = snip::FallbackEngine::process({"my_custom_tool"}, raw, 0);

    REQUIRE(res.text.find("Progress: 10%") == std::string::npos);
    REQUIRE(res.text.find("Progress: 50%") == std::string::npos);
    REQUIRE(res.text.find("Progress: 100%\nCompilation finished.") != std::string::npos);
}

TEST_CASE("FallbackEngine collapses repeated status and polling lines", "[fallback]") {
    std::string raw = 
        "Waiting for DB lock...\n"
        "Waiting for DB lock...\n"
        "Waiting for DB lock...\n"
        "Lock acquired.\n";
    
    auto res = snip::FallbackEngine::process({"my_custom_tool"}, raw, 0);

    REQUIRE(res.text.find("Waiting for DB lock... [repeated x3]\nLock acquired.\n") != std::string::npos);
}

TEST_CASE("FallbackEngine compacts vertical whitespace while preserving indentation", "[fallback]") {
    std::string raw = 
        "    int main() {\n"
        "        // Setup\n"
        "\n"
        "\n"
        "\n"
        "        return 0;\n"
        "    }\n";

    std::string compacted = snip::FallbackEngine::compact_whitespace(raw, 1);

    REQUIRE(compacted == 
        "    int main() {\n"
        "        // Setup\n"
        "\n"
        "        return 0;\n"
        "    }\n"
    );
}

TEST_CASE("FallbackEngine isolates compiler errors with context on non-zero exit", "[fallback]") {
    std::string raw =
        "Compiling module A...\n"
        "Compiling module B...\n"
        "Compiling module C...\n"
        "In file included from module_c.cpp:4:\n"
        "include/types.h:42:10: error: unknown type name 'BadType'\n"
        "   42 |   BadType value;\n"
        "      |   ^~~~~~~\n"
        "1 error generated.\n"
        "ninja: build stopped: subcommand failed.\n";

    auto res = snip::FallbackEngine::process({"custom_build"}, raw, 1);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("Compiling module A") == std::string::npos);
    REQUIRE(res.text.find("Compiling module B") == std::string::npos);
    REQUIRE(res.text.find("error: unknown type name 'BadType'") != std::string::npos);
    REQUIRE(res.text.find("BadType value;") != std::string::npos);
    REQUIRE(res.text.find("ninja: build stopped: subcommand failed.") != std::string::npos);
}

TEST_CASE("FallbackEngine isolates Python Traceback on non-zero exit", "[fallback]") {
    std::string raw =
        "Starting background worker 1...\n"
        "Starting background worker 2...\n"
        "Starting background worker 3...\n"
        "Traceback (most recent call last):\n"
        "  File \"job.py\", line 15, in <module>\n"
        "    run_worker()\n"
        "  File \"job.py\", line 8, in run_worker\n"
        "    raise ConnectionError('Redis connection timed out')\n"
        "ConnectionError: Redis connection timed out\n";

    auto res = snip::FallbackEngine::process({"python3", "worker.py"}, raw, 1);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("Starting background worker 1") == std::string::npos);
    REQUIRE(res.text.find("Traceback (most recent call last):") != std::string::npos);
    REQUIRE(res.text.find("ConnectionError: Redis connection timed out") != std::string::npos);
}

TEST_CASE("FallbackEngine isolates Rust panic on non-zero exit", "[fallback]") {
    std::string raw =
        "Compiling crate A v0.1.0\n"
        "Compiling crate B v0.1.0\n"
        "Building target...\n"
        "Finished dev profile in 2.1s\n"
        "Running custom binary...\n"
        "thread 'main' panicked at src/lib.rs:25:9:\n"
        "assertion failed: left == right\n"
        "  left: 401\n"
        " right: 200\n"
        "note: run with RUST_BACKTRACE=1\n";

    auto res = snip::FallbackEngine::process({"custom_rust_bin"}, raw, 101);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("Building target") == std::string::npos);
    REQUIRE(res.text.find("panicked at src/lib.rs:25:9:") != std::string::npos);
    REQUIRE(res.text.find("assertion failed: left == right") != std::string::npos);
}

TEST_CASE("FallbackEngine gracefully falls back to full sanitized output when no error anchors match", "[fallback]") {
    std::string raw = "License expired: please renew key at account portal.\n";
    auto res = snip::FallbackEngine::process({"proprietary_tool"}, raw, 1);

    // Invariant 1: Never Break Agent Execution - must not be empty!
    REQUIRE(res.text == "License expired: please renew key at account portal.\n");
    REQUIRE(res.was_compressed == false);
}

TEST_CASE("FallbackEngine compacts success logs when exceeding threshold", "[fallback]") {
    std::string raw;
    for (int i = 1; i <= 150; ++i) {
        raw += "Step " + std::to_string(i) + ": completed successfully\n";
    }

    auto res = snip::FallbackEngine::process({"long_batch_job"}, raw, 0);

    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("Step 1:") != std::string::npos);
    REQUIRE(res.text.find("Step 15:") != std::string::npos);
    REQUIRE(res.text.find("lines omitted") != std::string::npos);
    REQUIRE(res.text.find("Step 150:") != std::string::npos);
}

TEST_CASE("Dispatcher routes unrecognized commands to FallbackEngine", "[dispatcher]") {
    std::string raw =
        "Starting custom script...\n"
        "Step 1 ok\n"
        "Step 2 ok\n"
        "fatal error: database unreachable\n"
        "exit status 1\n";

    auto res = snip::Dispatcher::route_and_parse({"my_unknown_script.sh"}, raw, 1);

    REQUIRE(res.strategy_used == snip::PruneStrategyUsed::UniversalFallback);
    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("fatal error: database unreachable") != std::string::npos);
}

#include <catch2/catch_test_macros.hpp>
#include "snip/fallback_engine.hpp"
#include "snip/dispatcher.hpp"
#include "snip/runner.hpp"

TEST_CASE("FallbackEngine prune reduces consecutive repeated lines", "[pipe]") {
    std::string raw = "foo\nfoo\nfoo\n";
    auto res = snip::FallbackEngine::prune(raw, 0);

    CHECK(res.text == "foo [repeated x3]\n");
}

TEST_CASE("FallbackEngine prune strips ANSI escape sequences", "[pipe]") {
    std::string raw = "\033[32mSuccess\033[0m: task completed\n";
    auto res = snip::FallbackEngine::prune(raw, 0);

    CHECK(res.text.find("\033[") == std::string::npos);
    CHECK(res.text == "Success: task completed\n");
}

TEST_CASE("FallbackEngine prune handles empty input gracefully", "[pipe]") {
    auto res = snip::FallbackEngine::prune("", 0);
    CHECK(res.text.empty());
    CHECK(res.was_compressed == false);
}

TEST_CASE("FallbackEngine prune compacts excess blank lines", "[pipe]") {
    std::string raw = "line1\n\n\n\nline2\n";
    auto res = snip::FallbackEngine::prune(raw, 0);

    CHECK(res.text == "line1\n\nline2\n");
}

TEST_CASE("Dispatcher routes cat and read through FallbackEngine without skeletonizing", "[cat]") {
    std::string code =
        "#include <iostream>\n"
        "int compute(int x) {\n"
        "    int result = x * 2;\n"
        "    return result;\n"
        "}\n";

    auto cat_res = snip::Dispatcher::route_and_parse({"cat", "compute.cpp"}, code, 0);
    CHECK(cat_res.strategy_used == snip::PruneStrategyUsed::UniversalFallback);
    CHECK(cat_res.text.find("int result = x * 2;") != std::string::npos);
    CHECK(cat_res.text.find("/* ... */") == std::string::npos);

    auto read_res = snip::Dispatcher::route_and_parse({"read", "compute.cpp"}, code, 0);
    CHECK(read_res.strategy_used == snip::PruneStrategyUsed::UniversalFallback);
    CHECK(read_res.text.find("int result = x * 2;") != std::string::npos);
    CHECK(read_res.text.find("/* ... */") == std::string::npos);
}

#ifdef SNIP_EXECUTABLE_PATH
TEST_CASE("snip CLI processes piped input via stdin", "[pipe]") {
    std::string cmd = "printf 'foo\\nfoo\\nfoo\\n' | " + std::string(SNIP_EXECUTABLE_PATH);
    auto res = snip::ProcessRunner::execute({"sh", "-c", cmd});

    REQUIRE(res.exit_code == 0);
    CHECK(res.stdout_output.find("foo [repeated x3]") != std::string::npos);
}

TEST_CASE("snip CLI pipes and strips ANSI sequences via stdin", "[pipe]") {
    std::string cmd = "printf '\\033[31mError message\\033[0m\\n' | " + std::string(SNIP_EXECUTABLE_PATH);
    auto res = snip::ProcessRunner::execute({"sh", "-c", cmd});

    REQUIRE(res.exit_code == 0);
    CHECK(res.stdout_output.find("\033[") == std::string::npos);
    CHECK(res.stdout_output.find("Error message") != std::string::npos);
}

TEST_CASE("snip CLI prints injected SNIP_VERSION", "[version]") {
    std::string cmd = std::string(SNIP_EXECUTABLE_PATH) + " --version";
    auto res = snip::ProcessRunner::execute({"sh", "-c", cmd});

    REQUIRE(res.exit_code == 0);
    CHECK(res.stdout_output.find("snip version ") != std::string::npos);
#ifdef SNIP_VERSION
    CHECK(res.stdout_output.find(SNIP_VERSION) != std::string::npos);
#endif
}
#endif


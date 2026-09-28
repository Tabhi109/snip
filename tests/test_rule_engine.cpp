#include <catch2/catch_test_macros.hpp>
#include "snip/rule_engine.hpp"
#include "snip/dispatcher.hpp"
#include <fstream>

TEST_CASE("RuleEngine loads and matches columnar TOML rule", "[rule_engine]") {
    snip::RuleEngine& engine = snip::RuleEngine::instance();

    std::string toml_data = R"toml(
schema_version = "1.0"
name = "docker-ps"
description = "Prunes docker container listings"
strategy = "columnar"

[match]
binary = "docker"
subcommands = ["ps"]
exit_codes = [0]

[columnar]
header_row = 0
keep_columns = ["CONTAINER ID", "IMAGE", "STATUS"]
max_rows = 10
)toml";

    REQUIRE(engine.load_from_toml_string(toml_data, "test_docker.toml") == true);

    const auto* rule = engine.find_rule({"docker", "ps"}, 0);
    REQUIRE(rule != nullptr);
    REQUIRE(rule->name == "docker-ps");
    REQUIRE(rule->strategy == snip::RuleStrategyType::Columnar);
    REQUIRE(rule->columnar.keep_columns.size() == 3);

    // Mismatched subcommand should not match
    REQUIRE(engine.find_rule({"docker", "build"}, 0) == nullptr);

    // Mismatched exit code should not match
    REQUIRE(engine.find_rule({"docker", "ps"}, 1) == nullptr);
}

TEST_CASE("RuleEngine recovers gracefully from invalid TOML and broken regex", "[rule_engine]") {
    snip::RuleEngine& engine = snip::RuleEngine::instance();

    // 1. Broken TOML syntax
    std::string malformed_toml = "schema_version = '1.0'\n[match\n broken = ";
    REQUIRE(engine.load_from_toml_string(malformed_toml, "malformed.toml") == false);

    // 2. Broken regex pattern
    std::string invalid_regex_toml = R"toml(
schema_version = "1.0"
name = "bad-regex"
[match]
binary = "badtool"
strategy = "filter_replace"
[filter_replace]
drop_lines_matching = ["[unclosed-bracket"]
)toml";
    REQUIRE(engine.load_from_toml_string(invalid_regex_toml, "bad_regex.toml") == false);

    // Check that warning was logged to /tmp/snip_last_run.log
    std::ifstream log("/tmp/snip_last_run.log");
    REQUIRE(log.is_open());
    std::string line;
    bool found_warning = false;
    while (std::getline(log, line)) {
        if (line.find("snip:rule_engine WARN") != std::string::npos) {
            found_warning = true;
            break;
        }
    }
    REQUIRE(found_warning == true);
}

TEST_CASE("RuleEngine executes columnar strategy with column alignment and capping", "[rule_engine]") {
    snip::ColumnarStrategyConfig cfg;
    cfg.header_row = 0;
    cfg.keep_columns = {"CONTAINER ID", "IMAGE", "STATUS"};
    cfg.max_rows = 2;
    cfg.truncation_indicator = "[...+${count} containers omitted...]";

    std::string table = 
        "CONTAINER ID   IMAGE          COMMAND                  CREATED         STATUS         PORTS     NAMES\n"
        "a1b2c3d4e5f6   nginx:alpine   \"/docker-entrypoint.…\"   2 hours ago     Up 2 hours     80/tcp    web\n"
        "f6e5d4c3b2a1   redis:7        \"docker-entrypoint.s…\"   5 hours ago     Up 5 hours     6379/tcp  cache\n"
        "112233445566   postgres:16    \"docker-entrypoint.s…\"   10 hours ago    Up 10 hours    5432/tcp  db\n";

    std::string out = snip::RuleEngine::execute_columnar(cfg, table);

    REQUIRE(out.find("CONTAINER ID") != std::string::npos);
    REQUIRE(out.find("IMAGE") != std::string::npos);
    REQUIRE(out.find("STATUS") != std::string::npos);
    REQUIRE(out.find("COMMAND") == std::string::npos);
    REQUIRE(out.find("PORTS") == std::string::npos);
    REQUIRE(out.find("a1b2c3d4e5f6") != std::string::npos);
    REQUIRE(out.find("f6e5d4c3b2a1") != std::string::npos);
    REQUIRE(out.find("112233445566") == std::string::npos); // Capped at 2 rows
    REQUIRE(out.find("[...+1 containers omitted...]") != std::string::npos);
}

TEST_CASE("RuleEngine executes filter_replace strategy with transforms and context limits", "[rule_engine]") {
    snip::FilterReplaceStrategyConfig cfg;
    cfg.drop_patterns_str = {"^diff --git", "^index [0-9a-f]"};
    for (const auto& p : cfg.drop_patterns_str) {
        cfg.compiled_drop_patterns.emplace_back(p);
    }
    cfg.transforms.push_back({
        "^\\+\\+\\+ b/(.*)$",
        "file: $1",
        std::regex(R"(^\+\+\+ b/(.*)$)")
    });
    cfg.context_prefix = " ";
    cfg.max_consecutive_context = 1;

    std::string raw =
        "diff --git a/main.cpp b/main.cpp\n"
        "index abcdef..123456 100644\n"
        "+++ b/main.cpp\n"
        " int a = 1;\n"
        " int b = 2;\n"
        "+int c = 3;\n";

    std::string out = snip::RuleEngine::execute_filter_replace(cfg, raw);

    REQUIRE(out.find("diff --git") == std::string::npos);
    REQUIRE(out.find("index abcdef") == std::string::npos);
    REQUIRE(out.find("file: main.cpp") != std::string::npos);
    REQUIRE(out.find("int a = 1;") != std::string::npos);
    REQUIRE(out.find("int b = 2;") == std::string::npos); // Context collapsed
    REQUIRE(out.find("+int c = 3;") != std::string::npos);
}

TEST_CASE("RuleEngine executes hierarchy strategy with grouping and capping", "[rule_engine]") {
    snip::HierarchyStrategyConfig cfg;
    cfg.pattern_str = R"(^([^:\n]+):([0-9]+):(.*)$)";
    cfg.compiled_pattern = std::regex(cfg.pattern_str);
    cfg.header_template = "${file}";
    cfg.entry_template = "  ${line}: ${content}";
    cfg.max_matches_per_file = 2;
    cfg.max_total_matches = 3;
    cfg.truncation_template = "[+${remaining} more matches suppressed]";

    std::string search_raw =
        "src/a.cpp:10:int foo = 1;\n"
        "src/a.cpp:20:int bar = 2;\n"
        "src/a.cpp:30:int baz = 3;\n"
        "src/b.cpp:15:void run();\n"
        "src/b.cpp:25:void stop();\n";

    std::string out = snip::RuleEngine::execute_hierarchy(cfg, search_raw);

    REQUIRE(out.find("src/a.cpp\n") != std::string::npos);
    REQUIRE(out.find("  10: int foo = 1;\n") != std::string::npos);
    REQUIRE(out.find("  20: int bar = 2;\n") != std::string::npos);
    REQUIRE(out.find("  30: int baz = 3;\n") == std::string::npos); // Max per file is 2
    REQUIRE(out.find("src/b.cpp\n") != std::string::npos);
    REQUIRE(out.find("  15: void run();\n") != std::string::npos);
    REQUIRE(out.find("  25: void stop();\n") == std::string::npos); // Max total is 3
    REQUIRE(out.find("[+2 more matches suppressed]") != std::string::npos);
}

TEST_CASE("RuleEngine executes test_runner strategy on success and failure", "[rule_engine]") {
    snip::TestRunnerStrategyConfig cfg;
    cfg.success_pattern_str = R"((=+ [0-9]+ passed.*in [0-9\.]+s =+))";
    cfg.compiled_success_pattern = std::regex(cfg.success_pattern_str);
    cfg.success_template = "✓ ${1}";

    cfg.failure_section_start_str = "=== FAILURES ===";
    cfg.compiled_failure_section_start = std::regex(cfg.failure_section_start_str);
    cfg.failure_section_end_str = "=== short test summary info ===";
    cfg.compiled_failure_section_end = std::regex(cfg.failure_section_end_str);

    cfg.keep_patterns_str = {R"(^E\s+.*)", R"(^FAILED .*)"};
    for (const auto& p : cfg.keep_patterns_str) {
        cfg.compiled_keep_patterns.emplace_back(p);
    }
    cfg.summary_info_patterns_str = {R"(^FAILED .*)"};
    for (const auto& p : cfg.summary_info_patterns_str) {
        cfg.compiled_summary_info_patterns.emplace_back(p);
    }

    // Success path
    std::string succ_raw = "==== 10 passed in 0.22s ====\n";
    std::string succ_out = snip::RuleEngine::execute_test_runner(cfg, succ_raw, 0);
    REQUIRE(succ_out == "✓ ==== 10 passed in 0.22s ====\n");

    // Failure path
    std::string fail_raw =
        "test session starts\n"
        "=== FAILURES ===\n"
        "test_math\n"
        "E   assert 1 == 2\n"
        "=== short test summary info ===\n"
        "FAILED tests/test_math.py::test_math - assert 1 == 2\n";

    std::string fail_out = snip::RuleEngine::execute_test_runner(cfg, fail_raw, 1);
    REQUIRE(fail_out.find("test session starts") == std::string::npos);
    REQUIRE(fail_out.find("E   assert 1 == 2") != std::string::npos);
    REQUIRE(fail_out.find("FAILED tests/test_math.py::test_math") != std::string::npos);
}

TEST_CASE("Embedded rules docker-ps and kubectl-get are active out-of-the-box", "[rule_engine]") {
    snip::RuleEngine& engine = snip::RuleEngine::instance();

    const auto* docker_rule = engine.find_rule({"docker", "ps"}, 0);
    REQUIRE(docker_rule != nullptr);
    REQUIRE(docker_rule->name == "docker-ps");

    const auto* kubectl_rule = engine.find_rule({"kubectl", "get", "pods"}, 0);
    REQUIRE(kubectl_rule != nullptr);
    REQUIRE(kubectl_rule->name == "kubectl-get");
}

TEST_CASE("Dispatcher executes embedded declarative rule for docker ps", "[dispatcher]") {
    std::string docker_output =
        "CONTAINER ID   IMAGE          COMMAND                  CREATED         STATUS         PORTS     NAMES\n"
        "b3a98c0d12e4   postgres:15    \"docker-entrypoint.s…\"   2 days ago      Up 2 days      5432/tcp  pg_db\n";

    auto res = snip::Dispatcher::route_and_parse({"docker", "ps"}, docker_output, 0);

    REQUIRE(res.strategy_used == snip::PruneStrategyUsed::DeclarativeRule);
    REQUIRE(res.rule_name == "docker-ps");
    REQUIRE(res.was_compressed == true);
    REQUIRE(res.text.find("CONTAINER ID") != std::string::npos);
    REQUIRE(res.text.find("IMAGE") != std::string::npos);
    REQUIRE(res.text.find("STATUS") != std::string::npos);
    REQUIRE(res.text.find("COMMAND") == std::string::npos);
    REQUIRE(res.text.find("CREATED") == std::string::npos);
}

TEST_CASE("RuleEngine loads directory with overrides and ignores broken files", "[rule_engine]") {
    snip::RuleEngine& engine = snip::RuleEngine::instance();

    namespace fs = std::filesystem;
    fs::path temp_dir = fs::temp_directory_path() / "snip_test_rules";
    fs::create_directories(temp_dir);

    // Write a valid overriding rule
    std::ofstream good_file(temp_dir / "custom_docker.toml");
    good_file << R"toml(
schema_version = "1.0"
name = "custom-docker"
strategy = "columnar"
[match]
binary = "docker"
subcommands = ["ps"]
exit_codes = [0]
[columnar]
header_row = 0
keep_columns = ["CONTAINER ID"]
)toml";
    good_file.close();

    // Write an invalid TOML file in the same directory
    std::ofstream bad_file(temp_dir / "broken.toml");
    bad_file << "broken = [toml";
    bad_file.close();

    size_t loaded = engine.load_from_directory(temp_dir);
    REQUIRE(loaded == 1);

    // Custom docker rule should have overridden the default rule
    const auto* rule = engine.find_rule({"docker", "ps"}, 0);
    REQUIRE(rule != nullptr);
    REQUIRE(rule->name == "custom-docker");
    REQUIRE(rule->columnar.keep_columns.size() == 1);

    fs::remove_all(temp_dir);
}

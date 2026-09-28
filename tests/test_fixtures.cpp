#include <catch2/catch_test_macros.hpp>
#include "snip/dispatcher.hpp"
#include "snip/rule_engine.hpp"
#include <toml++/toml.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct FixtureMeta {
    std::vector<std::string> args;
    int exit_code{0};
};

std::string read_file_string(const fs::path& p) {
    std::ifstream file(p);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void write_file_string(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream file(p, std::ios::trunc);
    file << content;
}

fs::path resolve_fixtures_dir() {
#ifdef SNIP_FIXTURES_DIR
    if (fs::exists(SNIP_FIXTURES_DIR)) {
        return fs::path(SNIP_FIXTURES_DIR);
    }
#endif
    std::vector<fs::path> candidates = {
        "tests/fixtures",
        "../tests/fixtures",
        "../../tests/fixtures"
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) return fs::canonical(c);
    }
    return "";
}

FixtureMeta load_fixture_meta(const fs::path& meta_path, const std::string& stem) {
    FixtureMeta meta;
    if (fs::exists(meta_path)) {
        std::string content = read_file_string(meta_path);
        try {
            auto tbl = toml::parse(content, meta_path.string());
            if (auto args_arr = tbl["args"].as_array()) {
                for (const auto& elem : *args_arr) {
                    if (auto s = elem.value<std::string>()) {
                        meta.args.push_back(*s);
                    }
                }
            }
            meta.exit_code = static_cast<int>(tbl["exit_code"].value<int64_t>().value_or(0));
            if (!meta.args.empty()) {
                return meta;
            }
        } catch (...) {
            // Ignore parse error and fall back to filename splitting
        }
    }

    // Default deduce from stem (e.g. docker_ps -> ["docker", "ps"])
    std::string token;
    for (char c : stem) {
        if (c == '_') {
            if (!token.empty()) {
                meta.args.push_back(token);
                token.clear();
            }
        } else {
            token += c;
        }
    }
    if (!token.empty()) meta.args.push_back(token);
    meta.exit_code = 0;
    return meta;
}

std::string generate_diff_view(std::string_view actual, std::string_view expected) {
    std::stringstream ss;
    ss << "=== DIFF VIEW ===\n";
    ss << "--- Expected Snapshot ---\n" << expected;
    if (!expected.empty() && expected.back() != '\n') ss << "\n";
    ss << "+++ Actual Engine Output +++\n" << actual;
    if (!actual.empty() && actual.back() != '\n') ss << "\n";
    ss << "=========================\n";
    return ss.str();
}

} // namespace

TEST_CASE("Golden Fixtures Snapshot Suite", "[fixtures][golden]") {
    snip::RuleEngine::instance().reset();

    fs::path fixtures_dir = resolve_fixtures_dir();
    REQUIRE_FALSE(fixtures_dir.empty());

    fs::path raw_dir = fixtures_dir / "raw";
    fs::path expected_dir = fixtures_dir / "expected";
    fs::path meta_dir = fixtures_dir / "meta";

    REQUIRE(fs::exists(raw_dir));

    bool update_fixtures = false;
    const char* env_update = std::getenv("SNIP_UPDATE_FIXTURES");
    if (env_update && (std::string(env_update) == "1" || std::string(env_update) == "true")) {
        update_fixtures = true;
    }

    size_t fixtures_evaluated = 0;

    for (const auto& entry : fs::directory_iterator(raw_dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".txt") {
            continue;
        }

        std::string stem = entry.path().stem().string();
        DYNAMIC_SECTION("Fixture: " << stem) {
            std::string raw_input = read_file_string(entry.path());
            fs::path meta_path = meta_dir / (stem + ".toml");
            if (!fs::exists(meta_path)) {
                meta_path = meta_dir / (stem + ".json");
            }
            FixtureMeta meta = load_fixture_meta(meta_path, stem);

            // Execute engine through Dispatcher
            auto res = snip::Dispatcher::route_and_parse(meta.args, raw_input, meta.exit_code);

            fs::path expected_path = expected_dir / (stem + ".txt");

            if (update_fixtures || !fs::exists(expected_path)) {
                write_file_string(expected_path, res.text);
            }

            std::string expected_output = read_file_string(expected_path);

            if (res.text != expected_output) {
                std::string diff = generate_diff_view(res.text, expected_output);
                UNSCOPED_INFO(diff);
            }

            REQUIRE(res.text == expected_output);
        }
        fixtures_evaluated++;
    }

    REQUIRE(fixtures_evaluated >= 4);
}

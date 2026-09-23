#include "snip/test_parser.hpp"
#include "snip/dedup.hpp"
#include <sstream>
#include <vector>

namespace snip {

std::string TestParser::parse_pytest(std::string_view content, int exit_code) {
    std::istringstream stream{std::string(content)};
    std::string line;

    if (exit_code == 0) {
        std::string summary = "All tests passed.";
        while (std::getline(stream, line)) {
            if (line.find("passed in") != std::string_view::npos ||
                line.find("=====") != std::string_view::npos) {
                summary = line;
            }
        }
        return "✓ " + summary + "\n";
    }

    // Failure extraction
    std::string out;
    out.reserve(content.size() / 2);

    bool in_failure_section = false;
    bool in_summary_section = false;

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        // Skip pytest banner/platform lines
        if (sv.rfind("platform ", 0) == 0 || sv.rfind("rootdir: ", 0) == 0 ||
            sv.rfind("collected ", 0) == 0) {
            continue;
        }

        // Section markers
        if (sv.find("=== FAILURES ===") != std::string_view::npos ||
            sv.find("=== ERRORS ===") != std::string_view::npos) {
            in_failure_section = true;
            out.append("FAILURES:\n");
            continue;
        }

        if (sv.find("short test summary info") != std::string_view::npos) {
            in_failure_section = false;
            in_summary_section = true;
            out.append("SUMMARY:\n");
            continue;
        }

        // Always capture the final tally line (e.g., "= 1 failed, 40 passed in 0.12s =")
        if (sv.find(" failed") != std::string_view::npos && sv.find(" passed") != std::string_view::npos) {
            out.append(line).push_back('\n');
            continue;
        }

        if (in_summary_section) {
            if (sv.rfind("FAILED ", 0) == 0 || sv.rfind("ERROR ", 0) == 0) {
                out.append(line).push_back('\n');
            }
            continue;
        }

        if (in_failure_section) {
            // Filter out deep python internal virtualenv boilerplate
            if (sv.find("site-packages") != std::string_view::npos) {
                continue;
            }

            // Keep test name headers, assertion lines, and direct source frames
            if (sv.rfind("___", 0) == 0 || sv.rfind(">", 0) == 0 || 
                sv.rfind("E   ", 0) == 0 || sv.find(".py:") != std::string_view::npos) {
                out.append(line).push_back('\n');
            }
        }
    }

    return out.empty() ? std::string(content) : out;
}

std::string TestParser::parse_cargo_test(std::string_view content, int exit_code) {
    std::istringstream stream{std::string(content)};
    std::string line;

    if (exit_code == 0) {
        std::string summary = "All Rust cargo tests passed.";
        while (std::getline(stream, line)) {
            if (line.rfind("test result: ok.", 0) == 0) {
                summary = line;
            }
        }
        return "✓ " + summary + "\n";
    }

    std::string out;
    out.reserve(content.size() / 2);

    bool in_failures_block = false;

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        if (sv.rfind("failures:", 0) == 0) {
            in_failures_block = true;
            out.append("FAILURES:\n");
            continue;
        }

        if (sv.rfind("test result: FAILED.", 0) == 0) {
            in_failures_block = false;
            out.append(line).push_back('\n');
            continue;
        }

        if (in_failures_block) {
            // Skip thread backtrace noise if rust backtrace is verbose
            if (sv.find("stack backtrace:") != std::string_view::npos) {
                break;
            }
            // Keep panic lines, assertion mismatches, and test headers
            if (sv.find("panicked at") != std::string_view::npos ||
                sv.find("assertion `left == right` failed") != std::string_view::npos ||
                sv.find("left:") != std::string_view::npos ||
                sv.find("right:") != std::string_view::npos ||
                sv.rfind("---- ", 0) == 0) {
                out.append(line).push_back('\n');
            }
        }
    }

    return out.empty() ? std::string(content) : out;
}

std::string TestParser::parse_generic_test(std::string_view content, int exit_code) {
    if (exit_code == 0) {
        return "✓ PASS: Command exited with 0 (all checks passed).\n";
    }

    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size() / 2);

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        // Strip passing noise from generic outputs (like CTest)
        if (sv.find("Passed") != std::string_view::npos && 
            sv.find("100% tests passed") == std::string_view::npos) {
            continue;
        }

        if (sv.find("Error") != std::string_view::npos ||
            sv.find("FAILED") != std::string_view::npos ||
            sv.find("Exception") != std::string_view::npos ||
            sv.find("at ") != std::string_view::npos ||
            sv.find("File \"") != std::string_view::npos ||
            sv.find("Assertion") != std::string_view::npos ||
            sv.find("tests failed out of") != std::string_view::npos) {
            out.append(line).push_back('\n');
        }
    }

    return out.empty() ? std::string(content) : out;
}

ParseResult TestParser::parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int exit_code
) {
    ParseResult res;
    res.original_bytes = stdout_content.size();

    // Scan for subcommand/runner identity skipping flags
    std::string binary = cmd_args.empty() ? "" : cmd_args[0];
    std::string sub_cmd;
    for (size_t i = 1; i < cmd_args.size(); ++i) {
        if (!cmd_args[i].empty() && cmd_args[i][0] != '-') {
            sub_cmd = cmd_args[i];
            break;
        }
    }

    std::string compressed;

    if (binary == "pytest" || sub_cmd == "pytest") {
        compressed = parse_pytest(stdout_content, exit_code);
    } else if (binary == "cargo" && sub_cmd == "test") {
        compressed = parse_cargo_test(stdout_content, exit_code);
    } else {
        compressed = parse_generic_test(stdout_content, exit_code);
    }

    // Collapse repetitive stack trace lines or polling logs
    compressed = Deduplicator::collapse_repeated_lines(compressed);

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
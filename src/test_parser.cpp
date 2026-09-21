#include "snip/test_parser.hpp"
#include "snip/dedup.hpp"
#include <sstream>

namespace snip {

std::string TestParser::parse_pytest(std::string_view content, int exit_code) {
    if (exit_code == 0) {
        std::istringstream stream{std::string(content)};
        std::string line;
        std::string summary = "PASSED: All tests passed.";
        while (std::getline(stream, line)) {
            if (line.find("passed in") != std::string_view::npos ||
                line.find("=====") != std::string_view::npos) {
                summary = line;
            }
        }
        return "✓ " + summary + "\n";
    }

    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size());

    bool in_failure_section = false;

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        if (sv.find("=== FAILURES ===") != std::string_view::npos ||
            sv.find("=== ERRORS ===") != std::string_view::npos ||
            sv.find("short test summary info") != std::string_view::npos) {
            in_failure_section = true;
        }

        if (sv.find(" failed") != std::string_view::npos && sv.find(" passed") != std::string_view::npos) {
            out.append(line);
            out.push_back('\n');
            continue;
        }

        if (in_failure_section) {
            out.append(line);
            out.push_back('\n');
        }
    }

    return out.empty() ? std::string(content) : out;
}

std::string TestParser::parse_cargo_test(std::string_view content, int exit_code) {
    if (exit_code == 0) {
        return "✓ PASS: All Rust cargo tests passed.\n";
    }

    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size());

    bool in_failure_block = false;

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        if (sv.find("failures:") != std::string_view::npos ||
            sv.find("FAILED") != std::string_view::npos) {
            in_failure_block = true;
        }

        if (in_failure_block) {
            out.append(line);
            out.push_back('\n');
        }
    }

    return out.empty() ? std::string(content) : out;
}

std::string TestParser::parse_generic_test(std::string_view content, int exit_code) {
    if (exit_code == 0) {
        return "✓ PASS: Command exited with 0 (all checks/tests passed).\n";
    }

    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size());

    while (std::getline(stream, line)) {
        std::string_view sv = line;
        if (sv.find("Error") != std::string_view::npos ||
            sv.find("FAIL") != std::string_view::npos ||
            sv.find("Exception") != std::string_view::npos ||
            sv.find("at ") != std::string_view::npos ||
            sv.find("File \"") != std::string_view::npos) {
            out.append(line);
            out.push_back('\n');
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

    const std::string& binary = cmd_args[0];
    std::string compressed;

    if (binary == "pytest" || (cmd_args.size() > 1 && cmd_args[1] == "pytest")) {
        compressed = parse_pytest(stdout_content, exit_code);
    } else if (binary == "cargo" && cmd_args.size() > 1 && cmd_args[1] == "test") {
        compressed = parse_cargo_test(stdout_content, exit_code);
    } else {
        compressed = parse_generic_test(stdout_content, exit_code);
    }

    // Run deduplication pass over test output
    compressed = Deduplicator::collapse_repeated_lines(compressed);

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
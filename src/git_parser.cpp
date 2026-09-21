#include "snip/git_parser.hpp"
#include <sstream>
#include <vector>

namespace snip {

std::string GitParser::parse_status(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size());

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        // Skip Git's verbose interactive help instructions
        if (sv.find("(use \"git") != std::string_view::npos ||
            sv.find("nothing to commit, working tree clean") != std::string_view::npos) {
            continue;
        }

        // Drop unnecessary blank padding
        if (sv.empty() && !out.empty() && out.back() == '\n') {
            continue;
        }

        out.append(line);
        out.push_back('\n');
    }

    return out;
}

std::string GitParser::parse_diff(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size());

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        // Strip index metadata hashes like 'index e69de29..b7e1a3b 100644'
        if (sv.rfind("index ", 0) == 0) {
            continue;
        }

        out.append(line);
        out.push_back('\n');
    }

    return out;
}

ParseResult GitParser::parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int /*exit_code*/
) {
    ParseResult res;
    res.original_bytes = stdout_content.size();

    // Check if subcommand is "status" or "diff"
    std::string sub_cmd = (cmd_args.size() > 1) ? cmd_args[1] : "";

    std::string compressed;
    if (sub_cmd == "status") {
        compressed = parse_status(stdout_content);
    } else if (sub_cmd == "diff") {
        compressed = parse_diff(stdout_content);
    } else {
        // Fallback for git log, etc.
        compressed = std::string(stdout_content);
    }

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
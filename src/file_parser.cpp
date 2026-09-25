#include "snip/file_parser.hpp"
#include <sstream>
#include <vector>
#include <unordered_set>

namespace snip {

std::string FileParser::skeletonize_code(std::string_view code, std::string_view /*ext*/) {
    std::istringstream stream{std::string(code)};
    std::string line;
    std::string out;
    out.reserve(code.size() / 3);

    int brace_depth = 0;
    std::string signature_buffer;

    while (std::getline(stream, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;

        std::string_view sv = std::string_view(line).substr(first);

        // Preprocessor and includes always stay
        if (sv.rfind("#", 0) == 0 || sv.rfind("import ", 0) == 0 ||
            sv.rfind("from ", 0) == 0 || sv.rfind("package ", 0) == 0) {
            out.append(line).push_back('\n');
            continue;
        }

        // Top-level comments stay, inside-function comments are skipped
        if (sv.rfind("//", 0) == 0 || sv.rfind("/*", 0) == 0) {
            if (brace_depth <= 1) {
                out.append(line).push_back('\n');
            }
            continue;
        }

        // Count braces on this line
        int open_braces = 0;
        int close_braces = 0;
        for (char c : line) {
            if (c == '{') open_braces++;
            else if (c == '}') close_braces++;
        }

        // If we are at file or namespace level (depth 0 or 1)
        if (brace_depth <= 1) {
            // Namespace or struct/class declaration
            if (sv.find("namespace ") != std::string_view::npos ||
                sv.find("class ") != std::string_view::npos ||
                sv.find("struct ") != std::string_view::npos ||
                sv.find("enum ") != std::string_view::npos) {
                out.append(line).push_back('\n');
                brace_depth += (open_braces - close_braces);
                continue;
            }

            // Function/Method implementation
            if (line.find('(') != std::string_view::npos) {
                if (line.find('{') != std::string_view::npos) {
                    size_t b_pos = line.find('{');
                    out.append(line.substr(0, b_pos + 1)).append(" /* ... */ }\n");
                    brace_depth += (open_braces - close_braces);
                    continue;
                } else {
                    signature_buffer = line;
                }
            } else if (!signature_buffer.empty()) {
                signature_buffer += " " + line;
                if (line.find('{') != std::string_view::npos) {
                    size_t b_pos = signature_buffer.find('{');
                    out.append(signature_buffer.substr(0, b_pos + 1)).append(" /* ... */ }\n");
                    signature_buffer.clear();
                    brace_depth += (open_braces - close_braces);
                    continue;
                }
            } else if (open_braces == 0 && close_braces == 0) {
                out.append(line).push_back('\n');
            }
        }

        brace_depth += (open_braces - close_braces);
        if (brace_depth < 0) brace_depth = 0;
    }

    if (brace_depth == 0 && out.find('}') == std::string::npos) {
        out.append("}\n");
    }

    return out.empty() ? std::string(code) : out;
}

std::string FileParser::prune_tree(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size() / 2);

    const std::unordered_set<std::string> blacklist = {
        ".git", "node_modules", "build", "target", ".venv", "venv",
        "dist", "__pycache__", ".idea", ".vscode", "CMakeFiles"
    };

    while (std::getline(stream, line)) {
        bool skip = false;
        for (const auto& ignored : blacklist) {
            if (line.find(ignored) != std::string_view::npos) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        out.append(line).push_back('\n');
    }

    return out.empty() ? std::string(content) : out;
}

ParseResult FileParser::parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int /*exit_code*/
) {
    ParseResult res;
    res.original_bytes = stdout_content.size();

    std::string binary = cmd_args.empty() ? "" : cmd_args[0];
    std::string compressed;

    if (binary == "cat" || binary == "read") {
        compressed = skeletonize_code(stdout_content, "");
    } else {
        compressed = prune_tree(stdout_content);
    }

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
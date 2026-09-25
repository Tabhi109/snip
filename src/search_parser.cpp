#include "snip/search_parser.hpp"
#include <sstream>
#include <unordered_map>
#include <vector>

namespace snip {

std::string SearchParser::parse_ripgrep(std::string_view content) {
    if (content.empty()) return "No matches found.\n";

    std::istringstream stream{std::string(content)};
    std::string line;

    struct Match {
        std::string line_num;
        std::string code;
    };

    std::vector<std::string> file_order;
    std::unordered_map<std::string, std::vector<Match>> grouped_matches;
    size_t total_matches = 0;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        // ripgrep/grep format: "path/to/file:line:code" or "path/to/file:code"
        size_t first_colon = line.find(':');
        if (first_colon == std::string::npos) continue;

        size_t second_colon = line.find(':', first_colon + 1);
        std::string file_path;
        std::string line_num;
        std::string code;

        if (second_colon != std::string::npos) {
            // "path:line:code"
            file_path = line.substr(0, first_colon);
            line_num = line.substr(first_colon + 1, second_colon - first_colon - 1);
            
            size_t code_start = line.find_first_not_of(" \t", second_colon + 1);
            code = (code_start != std::string::npos) ? line.substr(code_start) : "";
        } else {
            // "path:code" (without line numbers)
            file_path = line.substr(0, first_colon);
            size_t code_start = line.find_first_not_of(" \t", first_colon + 1);
            code = (code_start != std::string::npos) ? line.substr(code_start) : "";
        }

        if (grouped_matches.find(file_path) == grouped_matches.end()) {
            file_order.push_back(file_path);
        }
        grouped_matches[file_path].push_back({line_num, code});
        total_matches++;
    }

    if (grouped_matches.empty()) {
        return std::string(content);
    }

    std::string out;
    out.reserve(content.size() / 2);

    size_t matches_shown = 0;
    const size_t MAX_MATCHES_TO_SHOW = 25;

    for (const auto& file : file_order) {
        if (matches_shown >= MAX_MATCHES_TO_SHOW) break;

        out.append(file).push_back('\n');
        for (const auto& m : grouped_matches[file]) {
            if (matches_shown >= MAX_MATCHES_TO_SHOW) break;
            out.append("  ");
            if (!m.line_num.empty()) {
                out.append(m.line_num).append(": ");
            }
            out.append(m.code).push_back('\n');
            matches_shown++;
        }
    }

    if (total_matches > matches_shown) {
        out.append("[+")
           .append(std::to_string(total_matches - matches_shown))
           .append(" more matches suppressed]\n");
    }

    return out;
}

ParseResult SearchParser::parse(
    const std::vector<std::string>& /*cmd_args*/,
    std::string_view stdout_content,
    int /*exit_code*/
) {
    ParseResult res;
    res.original_bytes = stdout_content.size();

    std::string compressed = parse_ripgrep(stdout_content);

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
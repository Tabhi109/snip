#include "snip/git_parser.hpp"
#include <sstream>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;

namespace snip {

std::string GitParser::parse_status(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string line;
    
    std::string branch = "HEAD";
    std::string branch_sync;
    std::vector<std::string> staged;
    std::vector<std::string> unstaged;
    std::vector<std::string> untracked;

    enum class Section { None, Staged, Unstaged, Untracked };
    Section current_section = Section::None;

    while (std::getline(stream, line)) {
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = line.find_last_not_of(" \t\r\n");
        std::string_view sv = std::string_view(line).substr(first, last - first + 1);

        // Branch name
        if (sv.rfind("On branch ", 0) == 0) {
            branch = std::string(sv.substr(10));
            continue;
        }

        // Branch divergence
        if (sv.find("Your branch is ahead") != std::string_view::npos ||
            sv.find("Your branch is behind") != std::string_view::npos ||
            sv.find("have diverged") != std::string_view::npos) {
            branch_sync = std::string(sv);
            continue;
        }

        // Section headers
        if (sv.find("Changes to be committed:") != std::string_view::npos) {
            current_section = Section::Staged;
            continue;
        }
        if (sv.find("Changes not staged for commit:") != std::string_view::npos) {
            current_section = Section::Unstaged;
            continue;
        }
        if (sv.find("Untracked files:") != std::string_view::npos) {
            current_section = Section::Untracked;
            continue;
        }

        // Skip Git advice & guidance sentences
        if (sv.front() == '(' && sv.back() == ')') continue;
        if (sv.find("no changes added to commit") != std::string_view::npos ||
            sv.find("nothing added to commit") != std::string_view::npos ||
            sv.find("nothing to commit") != std::string_view::npos) {
            continue;
        }

        // File lines
        if (current_section == Section::Staged || current_section == Section::Unstaged) {
            char status_code = 'M';
            std::string file_path;

            if (sv.rfind("modified:", 0) == 0) {
                status_code = 'M';
                file_path = std::string(sv.substr(9));
            } else if (sv.rfind("new file:", 0) == 0) {
                status_code = 'A';
                file_path = std::string(sv.substr(9));
            } else if (sv.rfind("deleted:", 0) == 0) {
                status_code = 'D';
                file_path = std::string(sv.substr(8));
            } else if (sv.rfind("renamed:", 0) == 0) {
                status_code = 'R';
                file_path = std::string(sv.substr(8));
            } else {
                continue;
            }

            size_t p_first = file_path.find_first_not_of(" \t");
            if (p_first != std::string::npos) {
                file_path = file_path.substr(p_first);
            }

            std::string entry = std::string(1, status_code) + " " + file_path;
            if (current_section == Section::Staged) {
                staged.push_back(std::move(entry));
            } else {
                unstaged.push_back(std::move(entry));
            }
        } else if (current_section == Section::Untracked) {
            untracked.emplace_back(sv);
        }
    }

    // Assemble dense output
    std::string out;
    out.reserve(content.size() / 3);

    out.append("branch: ").append(branch);
    if (!branch_sync.empty()) {
        out.append(" (").append(branch_sync).append(")");
    }
    out.push_back('\n');

    if (!staged.empty()) {
        out.append("[staged]\n");
        for (const auto& f : staged) {
            out.append(f).push_back('\n');
        }
    }

    if (!unstaged.empty()) {
        out.append("[unstaged]\n");
        for (const auto& f : unstaged) {
            out.append(f).push_back('\n');
        }
    }

    if (!untracked.empty()) {
        out.append("[untracked]\n");

        // Group files by parent directory
        std::unordered_map<std::string, std::vector<std::string>> dir_map;
        for (const auto& file : untracked) {
            fs::path p(file);
            std::string parent = p.has_parent_path() ? p.parent_path().string() : "";
            dir_map[parent].push_back(file);
        }

        for (const auto& [dir, files] : dir_map) {
            if (!dir.empty() && files.size() >= 3) {
                out.append("? [")
                   .append(std::to_string(files.size()))
                   .append(" files in ")
                   .append(dir)
                   .append("/]\n");
            } else {
                for (const auto& f : files) {
                    out.append("? ").append(f).push_back('\n');
                }
            }
        }
    }

    if (staged.empty() && unstaged.empty() && untracked.empty()) {
        out.append("clean\n");
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
        if (sv.rfind("index ", 0) == 0) continue;
        if (sv.rfind("diff --git", 0) == 0) continue;

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

    // Robust search for subcommand skipping CLI flags
    std::string sub_cmd;
    for (size_t i = 1; i < cmd_args.size(); ++i) {
        if (!cmd_args[i].empty() && cmd_args[i][0] != '-') {
            sub_cmd = cmd_args[i];
            break;
        }
    }

    std::string compressed;
    if (sub_cmd == "status") {
        compressed = parse_status(stdout_content);
    } else if (sub_cmd == "diff") {
        compressed = parse_diff(stdout_content);
    } else {
        compressed = std::string(stdout_content);
    }

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
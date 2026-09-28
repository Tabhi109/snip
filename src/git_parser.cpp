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

        if (sv.rfind("On branch ", 0) == 0) {
            branch = std::string(sv.substr(10));
            continue;
        }

        if (sv.find("Your branch is ahead") != std::string_view::npos ||
            sv.find("Your branch is behind") != std::string_view::npos ||
            sv.find("have diverged") != std::string_view::npos) {
            branch_sync = std::string(sv);
            continue;
        }

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

        if (sv.front() == '(' && sv.back() == ')') continue;
        if (sv.find("no changes added to commit") != std::string_view::npos ||
            sv.find("nothing added to commit") != std::string_view::npos ||
            sv.find("nothing to commit") != std::string_view::npos) {
            continue;
        }

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

    std::string out;
    out.reserve(content.size() / 4);

    out.append("## ").append(branch);
    if (!branch_sync.empty()) {
        out.append(" [").append(branch_sync).append("]");
    }
    out.push_back('\n');

    for (const auto& f : staged) {
        out.append(f).push_back('\n');
    }
    for (const auto& f : unstaged) {
        out.append(f).push_back('\n');
    }

    std::unordered_map<std::string, std::vector<std::string>> dir_map;
    for (const auto& file : untracked) {
        fs::path p(file);
        std::string parent = p.has_parent_path() ? p.parent_path().string() : "";
        dir_map[parent].push_back(file);
    }

    for (const auto& [dir, files] : dir_map) {
        if (!dir.empty() && files.size() >= 3) {
            out.append("?? [")
               .append(std::to_string(files.size()))
               .append(" files in ")
               .append(dir)
               .append("/]\n");
        } else {
            for (const auto& f : files) {
                out.append("?? ").append(f).push_back('\n');
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
    out.reserve(content.size() / 2);

    int context_run = 0;

    while (std::getline(stream, line)) {
        std::string_view sv = line;

        // 1. Skip metadata noise lines
        if (sv.rfind("diff --git", 0) == 0 ||
            sv.rfind("index ", 0) == 0 ||
            sv.rfind("new file mode", 0) == 0 ||
            sv.rfind("deleted file mode", 0) == 0 ||
            sv.rfind("similarity index", 0) == 0 ||
            sv.rfind("--- a/", 0) == 0) {
            continue;
        }

        // 2. Condense target file header: "+++ b/path" -> "file: path"
        if (sv.rfind("+++ b/", 0) == 0) {
            out.append("file: ").append(sv.substr(6)).push_back('\n');
            context_run = 0;
            continue;
        }

        // 3. Condense hunk coordinates: "@@ -45,12 +45,8 @@ void fn()" -> "@@ : void fn()"
        if (sv.rfind("@@ ", 0) == 0) {
            size_t second_at = sv.find("@@", 3);
            if (second_at != std::string_view::npos) {
                out.append("@@");
                if (second_at + 2 < sv.size()) {
                    out.append(sv.substr(second_at + 2));
                }
                out.push_back('\n');
                context_run = 0;
                continue;
            }
        }

        // 4. Added or deleted lines: always preserve
        if (!sv.empty() && (sv[0] == '+' || sv[0] == '-')) {
            out.append(line).push_back('\n');
            context_run = 0;
            continue;
        }

        // 5. Unchanged context lines: cap at 1 line
        if (!sv.empty() && sv[0] == ' ') {
            context_run++;
            if (context_run <= 1) {
                out.append(line).push_back('\n');
            }
            continue;
        }

        // 6. Any other line (e.g. "\ No newline at end of file")
        out.append(line).push_back('\n');
        context_run = 0;
    }

    return out;
}

std::string GitParser::parse_push(std::string_view /*content*/, std::string_view stderr_content, int exit_code) {
    if (exit_code != 0) {
        return std::string(stderr_content);
    }

    std::istringstream stream{std::string(stderr_content)};
    std::string line;
    std::string ref_summary;

    while (std::getline(stream, line)) {
        std::string_view sv = line;
        // Matches branch updates: "   e69de29..b7e1a3b  main -> main"
        if (sv.find("->") != std::string_view::npos) {
            size_t first = sv.find_first_not_of(" \t");
            ref_summary = (first != std::string_view::npos) ? std::string(sv.substr(first)) : line;
        }
        if (sv.find("Everything up-to-date") != std::string_view::npos) {
            return "ok: up-to-date\n";
        }
    }

    return ref_summary.empty() ? "ok\n" : "ok: " + ref_summary + "\n";
}

std::string GitParser::parse_commit(std::string_view content, int exit_code) {
    if (exit_code != 0) {
        return std::string(content);
    }

    std::istringstream stream{std::string(content)};
    std::string line;
    
    // First line typically: "[main e69de29] feat: add tokenizer"
    while (std::getline(stream, line)) {
        if (!line.empty() && line.front() == '[') {
            return "ok " + line + "\n";
        }
    }

    return "ok\n";
}

std::string GitParser::parse_log(std::string_view content) {
    std::istringstream stream{std::string(content)};
    std::string line;
    std::string out;
    out.reserve(content.size() / 2);

    std::string current_sha;
    std::string current_author;
    std::string current_date;
    std::string current_msg;

    auto flush_commit = [&]() {
        if (!current_sha.empty()) {
            out.append(current_sha.substr(0, 7)).append(" | ")
               .append(current_author).append(" | ")
               .append(current_msg).push_back('\n');
            current_sha.clear();
            current_author.clear();
            current_date.clear();
            current_msg.clear();
        }
    };

    while (std::getline(stream, line)) {
        std::string_view sv = line;
        if (sv.rfind("commit ", 0) == 0) {
            flush_commit();
            current_sha = std::string(sv.substr(7));
            continue;
        }
        if (sv.rfind("Author: ", 0) == 0) {
            size_t end_name = sv.find('<');
            if (end_name != std::string_view::npos) {
                current_author = std::string(sv.substr(8, end_name - 9));
            } else {
                current_author = std::string(sv.substr(8));
            }
            continue;
        }
        if (sv.rfind("Date: ", 0) == 0) {
            continue;
        }

        size_t first = sv.find_first_not_of(" \t");
        if (first != std::string_view::npos && !current_sha.empty() && current_msg.empty()) {
            current_msg = std::string(sv.substr(first));
        }
    }
    flush_commit();

    return out.empty() ? std::string(content) : out;
}

ParseResult GitParser::parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int exit_code
) {
    ParseResult res;
    res.original_bytes = stdout_content.size();

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
    } else if (sub_cmd == "commit") {
        compressed = parse_commit(stdout_content, exit_code);
    } else if (sub_cmd == "log") {
        compressed = parse_log(stdout_content);
    } else {
        compressed = std::string(stdout_content);
    }

    res.compressed_bytes = compressed.size();
    res.was_compressed = (res.compressed_bytes < res.original_bytes);
    res.text = std::move(compressed);

    return res;
}

} // namespace snip
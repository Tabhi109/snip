#include "snip/rule_engine.hpp"
#include "snip/embedded_rules.hpp"
#include "snip/dedup.hpp"
#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace snip {

namespace {

void log_warning(const std::string& msg) {
    std::ofstream log_file("/tmp/snip_last_run.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << "[snip:rule_engine WARN] " << msg << "\n";
    }
}

std::string trim_view(std::string_view sv) {
    size_t first = sv.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return "";
    size_t last = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(first, last - first + 1));
}

template <typename NodeView>
std::string get_string_or(const NodeView& node, std::string_view def = "") {
    if (auto val = node.template value<std::string>()) {
        return *val;
    }
    return std::string(def);
}

} // namespace

RuleEngine::RuleEngine() {
    load_embedded_rules();
    load_user_and_project_rules();
    initialized_ = true;
}

RuleEngine& RuleEngine::instance() {
    static RuleEngine engine;
    return engine;
}

void RuleEngine::clear() noexcept {
    rule_table_.clear();
}

void RuleEngine::reset() {
    clear();
    load_embedded_rules();
    load_user_and_project_rules();
}

size_t RuleEngine::rule_count() const noexcept {
    size_t count = 0;
    for (const auto& [_, rules] : rule_table_) {
        count += rules.size();
    }
    return count;
}

bool RuleEngine::register_rule(PruneRule rule) {
    if (rule.match.binary.empty()) {
        log_warning("Refusing to register rule without binary name: " + rule.name);
        return false;
    }
    rule_table_[rule.match.binary].push_back(std::move(rule));
    return true;
}

void RuleEngine::load_embedded_rules() {
    for (const auto& entry : EmbeddedRules::get_all()) {
        load_from_toml_string(entry.toml_content, std::string(entry.name) + " (embedded)");
    }
}

void RuleEngine::load_user_and_project_rules() {
    // 1. User config: ~/.config/snip/rules/
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");

    if (xdg_config && *xdg_config) {
        load_from_directory(std::filesystem::path(xdg_config) / "snip" / "rules");
    } else if (home && *home) {
        load_from_directory(std::filesystem::path(home) / ".config" / "snip" / "rules");
    }

    // 2. Project local: ./.snip/rules/
    load_from_directory(std::filesystem::path(".snip") / "rules");
}

size_t RuleEngine::load_from_directory(const std::filesystem::path& dir_path) {
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return 0;
    }

    size_t loaded = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec) && entry.path().extension() == ".toml") {
            if (load_from_file(entry.path())) {
                loaded++;
            }
        }
    }
    return loaded;
}

bool RuleEngine::load_from_file(const std::filesystem::path& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            log_warning("Unable to open rule file: " + file_path.string());
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return load_from_toml_string(buffer.str(), file_path.string());
    } catch (const std::exception& ex) {
        log_warning("Exception reading file " + file_path.string() + ": " + ex.what());
        return false;
    }
}

bool RuleEngine::load_from_toml_string(std::string_view toml_content, const std::string& source_name) {
    try {
        auto tbl = toml::parse(toml_content, source_name);

        PruneRule rule;
        rule.schema_version = get_string_or(tbl["schema_version"], "1.0");
        rule.name = get_string_or(tbl["name"], "unnamed");
        rule.description = get_string_or(tbl["description"], "");

        // Match table
        auto match_node = tbl["match"];
        if (!match_node.is_table()) {
            log_warning("Missing [match] table in rule: " + source_name);
            return false;
        }

        rule.match.binary = get_string_or(match_node["binary"], "");
        if (rule.match.binary.empty()) {
            log_warning("Missing binary in [match] table: " + source_name);
            return false;
        }

        // Subcommands can be string or array of strings
        if (auto sub_node = match_node["subcommands"]) {
            if (sub_node.is_array()) {
                for (const auto& elem : *sub_node.as_array()) {
                    if (auto str = elem.value<std::string>()) {
                        rule.match.subcommands.push_back(*str);
                    }
                }
            } else if (auto str = sub_node.value<std::string>()) {
                rule.match.subcommands.push_back(*str);
            }
        }

        // Exit codes can be int or array of ints
        if (auto code_node = match_node["exit_codes"]) {
            if (code_node.is_array()) {
                for (const auto& elem : *code_node.as_array()) {
                    if (auto val = elem.value<int>()) {
                        rule.match.exit_codes.push_back(*val);
                    }
                }
            } else if (auto val = code_node.value<int>()) {
                rule.match.exit_codes.push_back(*val);
            }
        }

        // Strategy parsing (support at root or after [match])
        std::string strat_str = get_string_or(tbl["strategy"], "");
        if (strat_str.empty() && match_node.is_table()) {
            strat_str = get_string_or(match_node["strategy"], "");
        }
        if (strat_str == "columnar") {
            rule.strategy = RuleStrategyType::Columnar;
            auto col_node = tbl["columnar"];
            if (col_node.is_table()) {
                rule.columnar.header_row = static_cast<size_t>(col_node["header_row"].value_or(0));
                rule.columnar.delimiter = get_string_or(col_node["delimiter"], R"(\s{2,})");
                rule.columnar.drop_empty_columns = col_node["drop_empty_columns"].value_or(true);
                rule.columnar.max_rows = static_cast<size_t>(col_node["max_rows"].value_or(50));
                rule.columnar.truncation_indicator = get_string_or(col_node["truncation_indicator"], "[...+records omitted...]");

                if (auto keep_node = col_node["keep_columns"].as_array()) {
                    for (const auto& elem : *keep_node) {
                        if (auto str = elem.value<std::string>()) {
                            rule.columnar.keep_columns.push_back(*str);
                        }
                    }
                }
            }
        } else if (strat_str == "filter_replace") {
            rule.strategy = RuleStrategyType::FilterReplace;
            auto fr_node = tbl["filter_replace"];
            if (fr_node.is_table()) {
                if (auto drop_node = fr_node["drop_lines_matching"].as_array()) {
                    for (const auto& elem : *drop_node) {
                        if (auto str = elem.value<std::string>()) {
                            rule.filter_replace.drop_patterns_str.push_back(*str);
                            rule.filter_replace.compiled_drop_patterns.emplace_back(*str, std::regex::optimize);
                        }
                    }
                }

                if (auto trans_node = fr_node["transforms"].as_array()) {
                    for (const auto& elem : *trans_node) {
                        if (const auto* t_tbl = elem.as_table()) {
                            std::string pat = get_string_or((*t_tbl)["pattern"], "");
                            std::string rep = get_string_or((*t_tbl)["replace"], "");
                            if (!pat.empty()) {
                                rule.filter_replace.transforms.push_back({
                                    pat,
                                    rep,
                                    std::regex(pat, std::regex::optimize)
                                });
                            }
                        }
                    }
                }

                if (auto ctx_node = fr_node["context_collapse"].as_table()) {
                    rule.filter_replace.context_prefix = get_string_or((*ctx_node)["context_prefix"], "");
                    rule.filter_replace.max_consecutive_context = static_cast<size_t>(
                        ctx_node->operator[]("max_consecutive_context").value_or(1)
                    );
                }
            }
        } else if (strat_str == "hierarchy") {
            rule.strategy = RuleStrategyType::Hierarchy;
            auto h_node = tbl["hierarchy"];
            if (h_node.is_table()) {
                rule.hierarchy.pattern_str = get_string_or(h_node["pattern"], "");
                if (!rule.hierarchy.pattern_str.empty()) {
                    rule.hierarchy.compiled_pattern = std::regex(rule.hierarchy.pattern_str, std::regex::optimize);
                }
                rule.hierarchy.header_template = get_string_or(h_node["header_template"], "${file}");
                rule.hierarchy.entry_template = get_string_or(h_node["entry_template"], "  ${line}: ${content}");
                rule.hierarchy.max_matches_per_file = static_cast<size_t>(h_node["max_matches_per_file"].value_or(10));
                rule.hierarchy.max_total_matches = static_cast<size_t>(h_node["max_total_matches"].value_or(25));
                rule.hierarchy.truncation_template = get_string_or(h_node["truncation_template"], "[+${remaining} more matches suppressed]");
            }
        } else if (strat_str == "test_runner") {
            rule.strategy = RuleStrategyType::TestRunner;
            auto tr_node = tbl["test_runner"];
            if (tr_node.is_table()) {
                if (auto succ_node = tr_node["on_success"].as_table()) {
                    rule.test_runner.success_pattern_str = get_string_or((*succ_node)["summary_pattern"], "");
                    if (!rule.test_runner.success_pattern_str.empty()) {
                        rule.test_runner.compiled_success_pattern = std::regex(rule.test_runner.success_pattern_str, std::regex::optimize);
                    }
                    rule.test_runner.success_template = get_string_or((*succ_node)["summary_template"], "✓ ${1}");
                    rule.test_runner.fallback_summary = get_string_or((*succ_node)["fallback_summary"], "✓ All tests passed.");
                }

                if (auto fail_node = tr_node["on_failure"].as_table()) {
                    rule.test_runner.failure_section_start_str = get_string_or((*fail_node)["failure_section_start"], "");
                    if (!rule.test_runner.failure_section_start_str.empty()) {
                        rule.test_runner.compiled_failure_section_start = std::regex(rule.test_runner.failure_section_start_str, std::regex::optimize);
                    }

                    rule.test_runner.failure_section_end_str = get_string_or((*fail_node)["failure_section_end"], "");
                    if (!rule.test_runner.failure_section_end_str.empty()) {
                        rule.test_runner.compiled_failure_section_end = std::regex(rule.test_runner.failure_section_end_str, std::regex::optimize);
                    }

                    if (auto keep_node = fail_node->operator[]("keep_patterns").as_array()) {
                        for (const auto& elem : *keep_node) {
                            if (auto str = elem.value<std::string>()) {
                                rule.test_runner.keep_patterns_str.push_back(*str);
                                rule.test_runner.compiled_keep_patterns.emplace_back(*str, std::regex::optimize);
                            }
                        }
                    }

                    if (auto drop_node = fail_node->operator[]("drop_patterns").as_array()) {
                        for (const auto& elem : *drop_node) {
                            if (auto str = elem.value<std::string>()) {
                                rule.test_runner.drop_patterns_str.push_back(*str);
                                rule.test_runner.compiled_drop_patterns.emplace_back(*str, std::regex::optimize);
                            }
                        }
                    }

                    if (auto sum_node = fail_node->operator[]("summary_info_patterns").as_array()) {
                        for (const auto& elem : *sum_node) {
                            if (auto str = elem.value<std::string>()) {
                                rule.test_runner.summary_info_patterns_str.push_back(*str);
                                rule.test_runner.compiled_summary_info_patterns.emplace_back(*str, std::regex::optimize);
                            }
                        }
                    }
                }
            }
        } else {
            log_warning("Unknown strategy '" + strat_str + "' in rule: " + source_name);
            return false;
        }

        return register_rule(std::move(rule));
    } catch (const toml::parse_error& err) {
        log_warning("TOML parse error in " + source_name + ": " + std::string(err.description()));
        return false;
    } catch (const std::regex_error& err) {
        log_warning("Regex compile error in " + source_name + ": " + err.what());
        return false;
    } catch (const std::exception& ex) {
        log_warning("Error processing rule " + source_name + ": " + ex.what());
        return false;
    }
}

const PruneRule* RuleEngine::find_rule(
    const std::vector<std::string>& cmd_args,
    int exit_code
) const noexcept {
    if (cmd_args.empty()) return nullptr;

    const std::string& binary = cmd_args[0];
    auto it = rule_table_.find(binary);
    if (it == rule_table_.end()) return nullptr;

    std::string sub_cmd;
    for (size_t i = 1; i < cmd_args.size(); ++i) {
        if (!cmd_args[i].empty() && cmd_args[i][0] != '-') {
            sub_cmd = cmd_args[i];
            break;
        }
    }

    // Iterate rules in reverse so newly registered user/project rules override embedded rules
    const auto& rules = it->second;
    for (auto rit = rules.rbegin(); rit != rules.rend(); ++rit) {
        const auto& rule = *rit;

        // 1. Exit code match
        if (!rule.match.exit_codes.empty()) {
            bool code_matches = false;
            for (int code : rule.match.exit_codes) {
                if (code == exit_code) {
                    code_matches = true;
                    break;
                }
            }
            if (!code_matches) continue;
        }

        // 2. Subcommand match
        if (!rule.match.subcommands.empty()) {
            bool sub_matches = false;
            for (const auto& s : rule.match.subcommands) {
                if (s == sub_cmd) {
                    sub_matches = true;
                    break;
                }
            }
            if (!sub_matches) continue;
        }

        return &rule;
    }

    return nullptr;
}

std::string RuleEngine::execute_columnar(
    const ColumnarStrategyConfig& cfg,
    std::string_view content
) {
    if (content.empty()) return "";

    std::vector<std::string_view> lines;
    size_t pos = 0;
    while (pos < content.size()) {
        size_t next = content.find('\n', pos);
        if (next == std::string_view::npos) {
            lines.push_back(content.substr(pos));
            break;
        }
        lines.push_back(content.substr(pos, next - pos));
        pos = next + 1;
    }

    if (lines.size() <= cfg.header_row) return std::string(content);

    std::string_view header_line = lines[cfg.header_row];

    struct ColDef {
        std::string name;
        size_t start{0};
        size_t end{std::string::npos};
    };

    std::vector<ColDef> columns;
    size_t idx = 0;

    while (idx < header_line.size()) {
        size_t token_start = header_line.find_first_not_of(" \t", idx);
        if (token_start == std::string_view::npos) break;

        size_t token_end = token_start + 1;
        while (token_end < header_line.size()) {
            if ((header_line[token_end] == ' ' || header_line[token_end] == '\t') &&
                token_end + 1 < header_line.size() &&
                (header_line[token_end + 1] == ' ' || header_line[token_end + 1] == '\t')) {
                break;
            }
            token_end++;
        }

        std::string col_name = trim_view(header_line.substr(token_start, token_end - token_start));
        if (!columns.empty()) {
            columns.back().end = token_start;
        }
        columns.push_back({ col_name, token_start, std::string::npos });
        idx = token_end;
    }

    if (columns.empty()) return std::string(content);

    std::vector<size_t> kept_indices;
    if (cfg.keep_columns.empty()) {
        for (size_t i = 0; i < columns.size(); ++i) kept_indices.push_back(i);
    } else {
        for (const auto& keep_name : cfg.keep_columns) {
            for (size_t i = 0; i < columns.size(); ++i) {
                if (columns[i].name == keep_name) {
                    kept_indices.push_back(i);
                    break;
                }
            }
        }
    }

    if (kept_indices.empty()) return std::string(content);

    std::vector<std::vector<std::string>> rows;
    size_t data_rows_seen = 0;

    for (size_t l = cfg.header_row + 1; l < lines.size(); ++l) {
        std::string_view line = lines[l];
        if (line.find_first_not_of(" \t\r\n") == std::string_view::npos) continue;

        data_rows_seen++;
        if (rows.size() >= cfg.max_rows) continue;

        std::vector<std::string> row_cells;
        for (size_t col_idx : kept_indices) {
            const auto& col = columns[col_idx];
            if (col.start >= line.size()) {
                row_cells.push_back("");
            } else {
                size_t len = (col.end != std::string::npos && col.end <= line.size()) ?
                             (col.end - col.start) : std::string_view::npos;
                row_cells.push_back(trim_view(line.substr(col.start, len)));
            }
        }
        rows.push_back(std::move(row_cells));
    }

    // Check empty columns to drop
    std::vector<bool> keep_col_active(kept_indices.size(), true);
    if (cfg.drop_empty_columns && !rows.empty()) {
        for (size_t c = 0; c < kept_indices.size(); ++c) {
            bool has_val = false;
            for (const auto& row : rows) {
                if (!row[c].empty()) {
                    has_val = true;
                    break;
                }
            }
            keep_col_active[c] = has_val;
        }
    }

    // Column widths
    std::vector<size_t> col_widths(kept_indices.size(), 0);
    for (size_t c = 0; c < kept_indices.size(); ++c) {
        if (!keep_col_active[c]) continue;
        col_widths[c] = columns[kept_indices[c]].name.size();
        for (const auto& row : rows) {
            col_widths[c] = std::max(col_widths[c], row[c].size());
        }
    }

    std::string out;
    out.reserve(content.size() / 2);

    // Header
    bool first_col = true;
    for (size_t c = 0; c < kept_indices.size(); ++c) {
        if (!keep_col_active[c]) continue;
        if (!first_col) out.append("   ");
        first_col = false;
        const std::string& name = columns[kept_indices[c]].name;
        out.append(name);
        if (c + 1 < kept_indices.size()) {
            size_t pad = (col_widths[c] > name.size()) ? (col_widths[c] - name.size()) : 0;
            out.append(pad, ' ');
        }
    }
    out.push_back('\n');

    // Data rows
    for (const auto& row : rows) {
        first_col = true;
        for (size_t c = 0; c < kept_indices.size(); ++c) {
            if (!keep_col_active[c]) continue;
            if (!first_col) out.append("   ");
            first_col = false;
            out.append(row[c]);
            if (c + 1 < kept_indices.size()) {
                size_t pad = (col_widths[c] > row[c].size()) ? (col_widths[c] - row[c].size()) : 0;
                out.append(pad, ' ');
            }
        }
        out.push_back('\n');
    }

    if (data_rows_seen > cfg.max_rows) {
        size_t omitted = data_rows_seen - cfg.max_rows;
        std::string indicator = cfg.truncation_indicator;
        size_t p = indicator.find("${count}");
        if (p != std::string::npos) {
            indicator.replace(p, 8, std::to_string(omitted));
        }
        out.append(indicator).push_back('\n');
    }

    return out;
}

std::string RuleEngine::execute_filter_replace(
    const FilterReplaceStrategyConfig& cfg,
    std::string_view content
) {
    if (content.empty()) return "";

    std::string out;
    out.reserve(content.size() / 2);

    size_t pos = 0;
    size_t context_count = 0;

    while (pos < content.size()) {
        size_t next = content.find('\n', pos);
        std::string_view line = (next == std::string_view::npos) ?
                                content.substr(pos) : content.substr(pos, next - pos);
        pos = (next == std::string_view::npos) ? content.size() : (next + 1);

        // 1. Drop patterns
        bool dropped = false;
        std::string l_str(line);
        for (const auto& reg : cfg.compiled_drop_patterns) {
            if (std::regex_search(l_str, reg)) {
                dropped = true;
                break;
            }
        }
        if (dropped) continue;

        // 2. Context collapse
        if (!cfg.context_prefix.empty()) {
            if (line.rfind(cfg.context_prefix, 0) == 0) {
                context_count++;
                if (context_count > cfg.max_consecutive_context) {
                    continue;
                }
            } else {
                context_count = 0;
            }
        }

        // 3. Transforms
        for (const auto& t : cfg.transforms) {
            if (std::regex_search(l_str, t.compiled_pattern)) {
                l_str = std::regex_replace(l_str, t.compiled_pattern, t.replacement);
            }
        }

        out.append(l_str).push_back('\n');
    }

    return out;
}

std::string RuleEngine::execute_hierarchy(
    const HierarchyStrategyConfig& cfg,
    std::string_view content
) {
    if (content.empty()) return "";

    std::vector<std::string> file_order;
    struct Entry { std::string line; std::string code; };
    std::unordered_map<std::string, std::vector<Entry>> groups;
    size_t total_matches = 0;

    size_t pos = 0;
    while (pos < content.size()) {
        size_t next = content.find('\n', pos);
        std::string_view l = (next == std::string_view::npos) ?
                             content.substr(pos) : content.substr(pos, next - pos);
        pos = (next == std::string_view::npos) ? content.size() : (next + 1);

        std::string line_str(l);
        std::smatch match;
        if (std::regex_search(line_str, match, cfg.compiled_pattern) && match.size() >= 4) {
            std::string file_path = match[1].str();
            std::string line_num = match[2].str();
            std::string code = trim_view(match[3].str());

            if (groups.find(file_path) == groups.end()) {
                file_order.push_back(file_path);
            }
            groups[file_path].push_back({ line_num, code });
            total_matches++;
        }
    }

    if (groups.empty()) return std::string(content);

    std::string out;
    out.reserve(content.size() / 2);

    size_t matches_shown = 0;
    for (const auto& file : file_order) {
        if (matches_shown >= cfg.max_total_matches) break;

        std::string h = cfg.header_template;
        size_t p = h.find("${file}");
        if (p != std::string::npos) h.replace(p, 7, file);
        out.append(h).push_back('\n');

        size_t file_count = 0;
        for (const auto& entry : groups[file]) {
            if (matches_shown >= cfg.max_total_matches || file_count >= cfg.max_matches_per_file) break;

            std::string e = cfg.entry_template;
            size_t p_l = e.find("${line}");
            if (p_l != std::string::npos) e.replace(p_l, 7, entry.line);
            size_t p_c = e.find("${content}");
            if (p_c != std::string::npos) e.replace(p_c, 10, entry.code);

            out.append(e).push_back('\n');
            matches_shown++;
            file_count++;
        }
    }

    if (total_matches > matches_shown) {
        size_t omitted = total_matches - matches_shown;
        std::string trunc = cfg.truncation_template;
        size_t p = trunc.find("${remaining}");
        if (p != std::string::npos) trunc.replace(p, 12, std::to_string(omitted));
        out.append(trunc).push_back('\n');
    }

    return out;
}

std::string RuleEngine::execute_test_runner(
    const TestRunnerStrategyConfig& cfg,
    std::string_view content,
    int exit_code
) {
    if (content.empty()) return "";

    std::istringstream stream{std::string(content)};
    std::string line;

    if (exit_code == 0) {
        std::string summary = cfg.fallback_summary;
        while (std::getline(stream, line)) {
            std::smatch match;
            if (!cfg.success_pattern_str.empty() &&
                std::regex_search(line, match, cfg.compiled_success_pattern)) {
                if (match.size() > 1) {
                    std::string tmpl = cfg.success_template;
                    size_t p = tmpl.find("${1}");
                    if (p != std::string::npos) {
                        tmpl.replace(p, 4, match[1].str());
                        summary = tmpl;
                    } else {
                        summary = "✓ " + match[1].str();
                    }
                } else {
                    summary = line;
                }
            }
        }
        if (!summary.empty() && summary.back() != '\n') summary.push_back('\n');
        return summary;
    }

    // Failure extraction
    std::string out;
    out.reserve(content.size() / 2);

    bool in_failure_section = false;
    bool in_summary_section = false;

    while (std::getline(stream, line)) {
        std::smatch match;

        if (!cfg.failure_section_start_str.empty() &&
            std::regex_search(line, match, cfg.compiled_failure_section_start)) {
            in_failure_section = true;
            in_summary_section = false;
            out.append("FAILURES:\n");
            continue;
        }

        if (!cfg.failure_section_end_str.empty() &&
            std::regex_search(line, match, cfg.compiled_failure_section_end)) {
            in_failure_section = false;
            in_summary_section = true;
            out.append("SUMMARY:\n");
            continue;
        }

        // Summary info lines
        if (in_summary_section) {
            for (const auto& reg : cfg.compiled_summary_info_patterns) {
                if (std::regex_search(line, match, reg)) {
                    out.append(line).push_back('\n');
                    break;
                }
            }
            continue;
        }

        if (in_failure_section) {
            bool drop = false;
            for (const auto& reg : cfg.compiled_drop_patterns) {
                if (std::regex_search(line, match, reg)) {
                    drop = true;
                    break;
                }
            }
            if (drop) continue;

            for (const auto& reg : cfg.compiled_keep_patterns) {
                if (std::regex_search(line, match, reg)) {
                    out.append(line).push_back('\n');
                    break;
                }
            }
        }
    }

    return out.empty() ? std::string(content) : Deduplicator::collapse_repeated_lines(out);
}

ParseResult RuleEngine::execute_rule(
    const PruneRule& rule,
    std::string_view content,
    int exit_code
) const noexcept {
    try {
        std::string compressed;
        switch (rule.strategy) {
            case RuleStrategyType::Columnar:
                compressed = execute_columnar(rule.columnar, content);
                break;
            case RuleStrategyType::FilterReplace:
                compressed = execute_filter_replace(rule.filter_replace, content);
                break;
            case RuleStrategyType::Hierarchy:
                compressed = execute_hierarchy(rule.hierarchy, content);
                break;
            case RuleStrategyType::TestRunner:
                compressed = execute_test_runner(rule.test_runner, content, exit_code);
                break;
        }

        size_t orig_bytes = content.size();
        size_t comp_bytes = compressed.size();
        bool was_compressed = (comp_bytes < orig_bytes && !compressed.empty());

        return ParseResult(
            std::move(compressed),
            was_compressed,
            orig_bytes,
            comp_bytes,
            PruneStrategyUsed::DeclarativeRule,
            rule.name
        );
    } catch (const std::exception& ex) {
        log_warning("Error applying rule '" + rule.name + "': " + ex.what());
        return ParseResult(
            std::string(content),
            false,
            content.size(),
            content.size(),
            PruneStrategyUsed::Passthrough,
            "rule_exception_fallback"
        );
    }
}

} // namespace snip

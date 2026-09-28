#pragma once

#include "snip/parser.hpp"
#include <filesystem>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace snip {

enum class RuleStrategyType {
    Columnar,
    FilterReplace,
    Hierarchy,
    TestRunner
};

struct RuleMatchCriteria {
    std::string binary;
    std::vector<std::string> subcommands; // empty matches any subcommand
    std::vector<int> exit_codes;          // empty matches any exit code
};

struct ColumnarStrategyConfig {
    size_t header_row{0};
    std::string delimiter{R"(\s{2,})"};
    std::vector<std::string> keep_columns;
    bool drop_empty_columns{true};
    size_t max_rows{50};
    std::string truncation_indicator{"[...+records omitted...]"};
};

struct RegexTransform {
    std::string pattern_str;
    std::string replacement;
    std::regex compiled_pattern;
};

struct FilterReplaceStrategyConfig {
    std::vector<std::string> drop_patterns_str;
    std::vector<std::regex> compiled_drop_patterns;
    std::vector<RegexTransform> transforms;
    std::string context_prefix;
    size_t max_consecutive_context{1};
};

struct HierarchyStrategyConfig {
    std::string pattern_str;
    std::regex compiled_pattern;
    std::string header_template{"${file}"};
    std::string entry_template{"  ${line}: ${content}"};
    size_t max_matches_per_file{10};
    size_t max_total_matches{25};
    std::string truncation_template{"[+${remaining} more matches suppressed]"};
};

struct TestRunnerStrategyConfig {
    std::string success_pattern_str;
    std::regex compiled_success_pattern;
    std::string success_template{"✓ ${1}"};
    std::string fallback_summary{"✓ All tests passed."};

    std::string failure_section_start_str;
    std::regex compiled_failure_section_start;
    std::string failure_section_end_str;
    std::regex compiled_failure_section_end;

    std::vector<std::string> keep_patterns_str;
    std::vector<std::regex> compiled_keep_patterns;
    std::vector<std::string> drop_patterns_str;
    std::vector<std::regex> compiled_drop_patterns;
    std::vector<std::string> summary_info_patterns_str;
    std::vector<std::regex> compiled_summary_info_patterns;
};

struct PruneRule {
    std::string schema_version{"1.0"};
    std::string name;
    std::string description;
    RuleMatchCriteria match;
    RuleStrategyType strategy{RuleStrategyType::Columnar};

    ColumnarStrategyConfig columnar;
    FilterReplaceStrategyConfig filter_replace;
    HierarchyStrategyConfig hierarchy;
    TestRunnerStrategyConfig test_runner;
};

class RuleEngine {
public:
    static RuleEngine& instance();

    // Load from a TOML string (used for testing and embedded rules)
    bool load_from_toml_string(std::string_view toml_content, const std::string& source_name = "memory");

    // Load from a single file with defensive error logging
    bool load_from_file(const std::filesystem::path& file_path);

    // Load all *.toml files from a directory defensively
    size_t load_from_directory(const std::filesystem::path& dir_path);

    // Initialize standard embedded rules
    void load_embedded_rules();

    // Discover and load ~/.config/snip/rules/ and ./.snip/rules/
    void load_user_and_project_rules();

    // Register a pre-constructed rule
    bool register_rule(PruneRule rule);

    // Find the best matching rule for given CLI arguments and exit code
    const PruneRule* find_rule(
        const std::vector<std::string>& cmd_args,
        int exit_code
    ) const noexcept;

    // Apply rule to content
    ParseResult execute_rule(
        const PruneRule& rule,
        std::string_view content,
        int exit_code
    ) const noexcept;

    // Clear all loaded rules (useful for test resets)
    void clear() noexcept;

    // Reset to clean state (clears and reloads embedded and user/project rules)
    void reset();

    // Number of registered rules
    size_t rule_count() const noexcept;

    // Strategy executors (exposed for direct testing)
    static std::string execute_columnar(
        const ColumnarStrategyConfig& cfg,
        std::string_view content
    );

    static std::string execute_filter_replace(
        const FilterReplaceStrategyConfig& cfg,
        std::string_view content
    );

    static std::string execute_hierarchy(
        const HierarchyStrategyConfig& cfg,
        std::string_view content
    );

    static std::string execute_test_runner(
        const TestRunnerStrategyConfig& cfg,
        std::string_view content,
        int exit_code
    );

private:
    RuleEngine();
    bool initialized_{false};
    std::unordered_map<std::string, std::vector<PruneRule>> rule_table_;
};

} // namespace snip

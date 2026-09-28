#include "snip/dispatcher.hpp"
#include "snip/git_parser.hpp"
#include "snip/test_parser.hpp"
#include "snip/search_parser.hpp"
#include "snip/file_parser.hpp"
#include "snip/fallback_engine.hpp"
#include "snip/rule_engine.hpp"

namespace snip {

ParseResult Dispatcher::route_and_parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int exit_code
) {
    return route_and_parse(cmd_args, stdout_content, "", exit_code);
}

ParseResult Dispatcher::route_and_parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    std::string_view stderr_content,
    int exit_code
) {
    if (cmd_args.empty()) {
        return { std::string(stdout_content), false, stdout_content.size(), stdout_content.size() };
    }

    try {
        // Step 1: Check Declarative Rule Engine (Tier 1)
        const auto* rule = RuleEngine::instance().find_rule(cmd_args, exit_code);
        if (rule) {
            auto rule_res = RuleEngine::instance().execute_rule(*rule, stdout_content, exit_code);
            if (rule_res.was_compressed && !rule_res.text.empty()) {
                return rule_res;
            }
        }

        // Step 2: Legacy Domain Parsers (during migration transition)
        const std::string& binary = cmd_args[0];
        std::string sub_cmd;
        for (size_t i = 1; i < cmd_args.size(); ++i) {
            if (!cmd_args[i].empty() && cmd_args[i][0] != '-') {
                sub_cmd = cmd_args[i];
                break;
            }
        }

        std::unique_ptr<DomainParser> parser;

        if (binary == "git") {
            parser = std::make_unique<GitParser>();
        } 
        else if (binary == "rg" || binary == "grep") {
            parser = std::make_unique<SearchParser>();
        } 
        else if (binary == "find" || binary == "ls" || binary == "tree") {
            parser = std::make_unique<FileParser>();
        }
        else if (binary == "pytest" || sub_cmd == "pytest" ||
                   (binary == "cargo" && sub_cmd == "test") ||
                   (binary == "go" && sub_cmd == "test") ||
                   (binary == "npm" && (sub_cmd == "test" || sub_cmd == "t")) ||
                   binary == "ctest") {
            parser = std::make_unique<TestParser>();
        }

        if (parser) {
            return parser->parse(cmd_args, stdout_content, exit_code);
        }

        // Step 3: Tier 2: Universal Fallback Engine for all other tools
        return FallbackEngine::process(cmd_args, stdout_content, stderr_content, exit_code);
    } catch (...) {
        // Core Invariant 1: Never Break Agent Execution
        return {
            std::string(stdout_content),
            false,
            stdout_content.size(),
            stdout_content.size(),
            PruneStrategyUsed::Passthrough,
            "dispatcher_safety_catch"
        };
    }
}

} // namespace snip
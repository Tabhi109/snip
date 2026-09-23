#include "snip/dispatcher.hpp"
#include "snip/git_parser.hpp"
#include "snip/test_parser.hpp"

namespace snip {

ParseResult Dispatcher::route_and_parse(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int exit_code
) {
    if (cmd_args.empty()) {
        return { std::string(stdout_content), false, stdout_content.size(), stdout_content.size() };
    }

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
    } else if (binary == "pytest" || sub_cmd == "pytest" ||
               (binary == "cargo" && sub_cmd == "test") ||
               (binary == "go" && sub_cmd == "test") ||
               (binary == "npm" && (sub_cmd == "test" || sub_cmd == "t")) ||
               binary == "ctest") {
        parser = std::make_unique<TestParser>();
    }

    if (parser) {
        return parser->parse(cmd_args, stdout_content, exit_code);
    }

    return {
        std::string(stdout_content),
        false,
        stdout_content.size(),
        stdout_content.size()
    };
}

} // namespace snip
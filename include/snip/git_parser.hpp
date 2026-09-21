#pragma once

#include "snip/parser.hpp"

namespace snip {

class GitParser : public DomainParser {
public:
    ParseResult parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    ) override;

private:
    static std::string parse_status(std::string_view content);
    static std::string parse_diff(std::string_view content);
};

} // namespace snip
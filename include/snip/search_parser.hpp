#pragma once

#include "snip/parser.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace snip {

class SearchParser : public DomainParser {
public:
    ParseResult parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    ) override;

    static std::string parse_ripgrep(std::string_view content);
};

} // namespace snip
#pragma once

#include "snip/parser.hpp"

namespace snip {

class TestParser : public DomainParser {
public:
    ParseResult parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    ) override;

private:
    static std::string parse_cargo_test(std::string_view content, int exit_code);
    static std::string parse_pytest(std::string_view content, int exit_code);
    static std::string parse_generic_test(std::string_view content, int exit_code);
};

} // namespace snip
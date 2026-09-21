#pragma once

#include "snip/parser.hpp"
#include <memory>
#include <string>
#include <vector>

namespace snip {

class Dispatcher {
public:
    static ParseResult route_and_parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    );
};

} // namespace snip
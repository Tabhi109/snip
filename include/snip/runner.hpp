#pragma once

#include <string>
#include <vector>

namespace snip {

struct CommandResult {
    int exit_code{0};
    std::string stdout_output;
    std::string stderr_output;
};

class ProcessRunner {
public:
    static CommandResult execute(const std::vector<std::string>& args);
};

} // namespace snip
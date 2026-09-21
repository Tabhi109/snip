#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace snip {

struct ParseResult {
    std::string text;
    bool was_compressed{false};
    size_t original_bytes{0};
    size_t compressed_bytes{0};
};

class DomainParser {
public:
    virtual ~DomainParser() = default;
    
    // Each domain parser implements this method
    virtual ParseResult parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    ) = 0;
};

} // namespace snip
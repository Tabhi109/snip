#pragma once

#include "snip/parser.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace snip {

struct FallbackOptions {
    bool strip_ansi{true};
    bool deduplicate_lines{true};
    bool compact_whitespace{true};
    size_t max_consecutive_blank_lines{1};
    size_t head_lines_on_success{15};
    size_t tail_lines_on_success{25};
    size_t success_threshold_lines{100};
    size_t error_context_before{2};
    size_t error_context_after{3};
};

class FallbackEngine {
public:
    // Process process output with universal heuristics
    static ParseResult process(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        std::string_view stderr_content,
        int exit_code,
        const FallbackOptions& options = {}
    ) noexcept;

    // Convenience overload when single stream is passed
    static ParseResult process(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code,
        const FallbackOptions& options = {}
    ) noexcept;

    // Sub-algorithms exposed for direct testing and modular reuse
    static std::string compact_whitespace(std::string_view input, size_t max_blank = 1);
    
    static std::string isolate_error_context(
        std::string_view input,
        size_t context_before = 2,
        size_t context_after = 3
    );

    static std::string compact_success_log(
        std::string_view input,
        size_t head_lines = 15,
        size_t tail_lines = 25,
        size_t threshold = 100
    );
};

} // namespace snip

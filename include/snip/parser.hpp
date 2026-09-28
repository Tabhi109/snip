#pragma once

#include <string>
#include <string_view>
#include <vector>
namespace snip {

enum class PruneStrategyUsed {
    DeclarativeRule,
    HardcodedDomain,
    UniversalFallback,
    Passthrough
};

struct ParseResult {
    std::string text;
    bool was_compressed{false};
    size_t original_bytes{0};
    size_t compressed_bytes{0};
    PruneStrategyUsed strategy_used{PruneStrategyUsed::Passthrough};
    std::string rule_name{};

    ParseResult() = default;
    ParseResult(std::string t, bool comp, size_t orig_b, size_t comp_b,
                PruneStrategyUsed strat = PruneStrategyUsed::Passthrough,
                std::string r_name = "")
        : text(std::move(t)), was_compressed(comp), original_bytes(orig_b),
          compressed_bytes(comp_b), strategy_used(strat), rule_name(std::move(r_name)) {}
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
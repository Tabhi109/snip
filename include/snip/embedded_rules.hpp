#pragma once

#include <string_view>
#include <vector>

namespace snip {

struct EmbeddedRuleEntry {
    std::string_view name;
    std::string_view toml_content;
};

class EmbeddedRules {
public:
    static const std::vector<EmbeddedRuleEntry>& get_all();
};

} // namespace snip

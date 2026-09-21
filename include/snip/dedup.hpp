#pragma once

#include <string>
#include <string_view>

namespace snip {

class Deduplicator {
public:
    // Scans lines and collapses consecutive duplicate lines into "line [repeated xN]"
    static std::string collapse_repeated_lines(std::string_view input);
};

} // namespace snip
#pragma once

#include <string>
#include <string_view>

namespace snip {

class StreamSanitizer {
public:
    static std::string sanitize(std::string_view input);
};

} // namespace snip
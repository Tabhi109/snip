#include "snip/dedup.hpp"
#include <sstream>

namespace snip {

std::string Deduplicator::collapse_repeated_lines(std::string_view input) {
    std::istringstream stream{std::string(input)};
    std::string line;
    std::string out;
    out.reserve(input.size());

    std::string last_line;
    size_t repeat_count = 0;

    auto flush_last_line = [&]() {
        if (last_line.empty() && repeat_count == 0) return;

        out.append(last_line);
        if (repeat_count > 1) {
            out.append(" [repeated x");
            out.append(std::to_string(repeat_count));
            out.append("]");
        }
        out.push_back('\n');
    };

    while (std::getline(stream, line)) {
        if (line == last_line && !line.empty()) {
            repeat_count++;
        } else {
            flush_last_line();
            last_line = line;
            repeat_count = 1;
        }
    }
    flush_last_line();

    return out;
}

} // namespace snip
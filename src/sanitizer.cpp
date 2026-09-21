#include "snip/sanitizer.hpp"

namespace snip {

std::string StreamSanitizer::sanitize(std::string_view input) {
    std::string out;
    out.reserve(input.size());

    size_t i = 0;
    size_t line_start = 0;

    while (i < input.size()) {
        char c = input[i];

        // 1. Strip ANSI Escape sequences (\033[ ... [command-char])
        if (c == '\033' && (i + 1 < input.size()) && input[i + 1] == '[') {
            i += 2;
            while (i < input.size() && (input[i] < '@' || input[i] > '~')) {
                i++;
            }
            if (i < input.size()) {
                i++;
            }
            continue;
        }

        // 2. Carriage Return (\r) line rewinding for terminal spinners
        if (c == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                out.push_back('\n');
                i += 2;
                line_start = out.size();
                continue;
            } else {
                out.resize(line_start);
                i++;
                continue;
            }
        }

        // 3. Newline tracking
        if (c == '\n') {
            out.push_back('\n');
            line_start = out.size();
            i++;
            continue;
        }

        out.push_back(c);
        i++;
    }

    return out;
}

} // namespace snip
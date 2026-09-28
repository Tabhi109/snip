#include "snip/sanitizer.hpp"

namespace snip {

std::string StreamSanitizer::sanitize(std::string_view input) {
    std::string out;
    out.reserve(input.size());

    size_t i = 0;
    size_t line_start = 0;

    while (i < input.size()) {
        char c = input[i];

        // 1. Strip ANSI CSI Escape sequences (\033[ ... [command-char])
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

        // 2. Strip OSC sequences (\033] ... \007 or \033] ... \033\\)
        if (c == '\033' && (i + 1 < input.size()) && input[i + 1] == ']') {
            i += 2;
            while (i < input.size()) {
                if (input[i] == '\007') {
                    i++;
                    break;
                }
                if (input[i] == '\033' && i + 1 < input.size() && input[i + 1] == '\\') {
                    i += 2;
                    break;
                }
                i++;
            }
            continue;
        }

        // 3. Strip charset / single character escape sequences (e.g. \033(B)
        if (c == '\033' && (i + 1 < input.size())) {
            char next = input[i + 1];
            if (next == '(' || next == ')' || next == '*' || next == '+') {
                i += (i + 2 < input.size()) ? 3 : 2;
                continue;
            }
            if (next >= 0x40 && next <= 0x5F) {
                i += 2;
                continue;
            }
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
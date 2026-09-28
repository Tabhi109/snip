#include "snip/fallback_engine.hpp"
#include "snip/sanitizer.hpp"
#include "snip/dedup.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace snip {

namespace {

bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) {
            return std::tolower(static_cast<unsigned char>(ch1)) ==
                   std::tolower(static_cast<unsigned char>(ch2));
        }
    );
    return it != haystack.end();
}

bool is_error_anchor(std::string_view line) {
    if (line.empty()) return false;

    // Guard against false positives like "0 errors, 0 failed" in test summaries
    if (contains_case_insensitive(line, "0 failed") ||
        contains_case_insensitive(line, "0 errors") ||
        contains_case_insensitive(line, "0 passed; 0 failed")) {
        return false;
    }

    static const std::string_view anchors[] = {
        "error:", "error [", "[error]",
        "fatal:", "fatal error", "[fatal]",
        "traceback (most recent call last):",
        "panicked at",
        "assertionerror", "assertion failed", "assertion `",
        "undefined reference",
        "segmentation fault", "sigsegv",
        "exception:", "unhandled exception",
        "syntaxerror",
        "fail:"
    };

    for (const auto& a : anchors) {
        if (contains_case_insensitive(line, a)) {
            return true;
        }
    }

    // Direct uppercase marker checks
    if (line.find("FAILED") != std::string_view::npos ||
        line.find("ERROR") != std::string_view::npos) {
        return true;
    }

    return false;
}

} // namespace

std::string FallbackEngine::compact_whitespace(std::string_view input, size_t max_blank) {
    if (input.empty()) return "";

    std::string out;
    out.reserve(input.size());

    size_t start = 0;
    size_t blank_count = 0;
    bool has_trailing_newline = (input.back() == '\n');

    while (start < input.size()) {
        size_t end = input.find('\n', start);
        std::string_view line;
        bool is_last_line = (end == std::string_view::npos);

        if (is_last_line) {
            line = input.substr(start);
            start = input.size();
        } else {
            line = input.substr(start, end - start);
            start = end + 1;
        }

        // Trim trailing whitespace (spaces, tabs, carriage returns)
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }

        if (line.empty()) {
            blank_count++;
            if (blank_count <= max_blank) {
                out.push_back('\n');
            }
        } else {
            blank_count = 0;
            out.append(line);
            if (!is_last_line || has_trailing_newline) {
                out.push_back('\n');
            }
        }
    }

    return out;
}

std::string FallbackEngine::isolate_error_context(
    std::string_view input,
    size_t context_before,
    size_t context_after
) {
    if (input.empty()) return "";

    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < input.size()) {
        size_t end = input.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(input.substr(start));
            break;
        }
        lines.push_back(input.substr(start, end - start));
        start = end + 1;
    }

    if (lines.empty()) return "";

    std::vector<bool> keep(lines.size(), false);
    size_t anchor_count = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        if (is_error_anchor(lines[i])) {
            anchor_count++;
            size_t w_start = (i >= context_before) ? (i - context_before) : 0;
            size_t w_end = std::min(lines.size() - 1, i + context_after);
            for (size_t k = w_start; k <= w_end; ++k) {
                keep[k] = true;
            }
        }
    }

    // Invariant: If no error anchors matched, return empty so FallbackEngine preserves full text
    if (anchor_count == 0) {
        return "";
    }

    // Keep the final summary line if it indicates stoppage/failure
    if (!lines.empty() && !keep.back()) {
        std::string_view last = lines.back();
        if (contains_case_insensitive(last, "stop") ||
            contains_case_insensitive(last, "fail") ||
            contains_case_insensitive(last, "exit status")) {
            keep.back() = true;
        }
    }

    std::string out;
    out.reserve(input.size());

    bool in_omission = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (keep[i]) {
            in_omission = false;
            out.append(lines[i]).push_back('\n');
        } else {
            if (!in_omission) {
                out.append("[...]\n");
                in_omission = true;
            }
        }
    }

    return out;
}

std::string FallbackEngine::compact_success_log(
    std::string_view input,
    size_t head_lines,
    size_t tail_lines,
    size_t threshold
) {
    if (input.empty()) return "";

    std::vector<std::string_view> lines;
    size_t start = 0;
    while (start < input.size()) {
        size_t end = input.find('\n', start);
        if (end == std::string_view::npos) {
            lines.push_back(input.substr(start));
            break;
        }
        lines.push_back(input.substr(start, end - start));
        start = end + 1;
    }

    if (lines.size() <= threshold || lines.size() <= (head_lines + tail_lines)) {
        return std::string(input);
    }

    std::string out;
    out.reserve(input.size());

    // Emit head lines
    for (size_t i = 0; i < head_lines; ++i) {
        out.append(lines[i]).push_back('\n');
    }

    // Emit omission indicator
    size_t omitted = lines.size() - head_lines - tail_lines;
    out.append("[... ")
       .append(std::to_string(omitted))
       .append(" lines omitted ...]\n");

    // Emit tail lines
    for (size_t i = lines.size() - tail_lines; i < lines.size(); ++i) {
        out.append(lines[i]).push_back('\n');
    }

    return out;
}

ParseResult FallbackEngine::process(
    const std::vector<std::string>& /*cmd_args*/,
    std::string_view stdout_content,
    std::string_view stderr_content,
    int exit_code,
    const FallbackOptions& options
) noexcept {
    try {
        std::string combined_raw;
        std::string_view raw_view;

        if (!stdout_content.empty() && !stderr_content.empty()) {
            combined_raw.reserve(stdout_content.size() + stderr_content.size() + 1);
            combined_raw.append(stdout_content);
            combined_raw.push_back('\n');
            combined_raw.append(stderr_content);
            raw_view = combined_raw;
        } else if (!stdout_content.empty()) {
            raw_view = stdout_content;
        } else {
            raw_view = stderr_content;
        }

        if (raw_view.empty()) {
            return ParseResult("", false, 0, 0, PruneStrategyUsed::Passthrough, "fallback_empty");
        }

        size_t orig_bytes = raw_view.size();
        std::string text;

        // 1. ANSI & Terminal escape sanitization
        if (options.strip_ansi) {
            text = StreamSanitizer::sanitize(raw_view);
        } else {
            text = std::string(raw_view);
        }

        // 2. Consecutive duplicate line collapse
        if (options.deduplicate_lines) {
            text = Deduplicator::collapse_repeated_lines(text);
        }

        // 3. Whitespace compaction
        if (options.compact_whitespace) {
            text = compact_whitespace(text, options.max_consecutive_blank_lines);
        }

        // 4. Exit-code-aware pruning
        if (exit_code != 0) {
            std::string isolated = isolate_error_context(
                text,
                options.error_context_before,
                options.error_context_after
            );
            // If error anchors were identified, use the isolated error block;
            // Otherwise preserve full sanitized text so nothing is lost!
            if (!isolated.empty()) {
                text = std::move(isolated);
            }
        } else {
            text = compact_success_log(
                text,
                options.head_lines_on_success,
                options.tail_lines_on_success,
                options.success_threshold_lines
            );
        }

        size_t comp_bytes = text.size();
        bool was_compressed = (comp_bytes < orig_bytes);

        return ParseResult(
            std::move(text),
            was_compressed,
            orig_bytes,
            comp_bytes,
            PruneStrategyUsed::UniversalFallback,
            "universal_fallback"
        );
    } catch (...) {
        // Core Invariant 1: Never Break Agent Execution
        return ParseResult(
            std::string(stdout_content),
            false,
            stdout_content.size(),
            stdout_content.size(),
            PruneStrategyUsed::Passthrough,
            "safety_catch"
        );
    }
}

ParseResult FallbackEngine::process(
    const std::vector<std::string>& cmd_args,
    std::string_view stdout_content,
    int exit_code,
    const FallbackOptions& options
) noexcept {
    return process(cmd_args, stdout_content, "", exit_code, options);
}

} // namespace snip

#include <iostream>
#include <vector>
#include <string>
#include "snip/runner.hpp"
#include "snip/sanitizer.hpp"
#include "snip/dispatcher.hpp"
#include "snip/cache.hpp"
#include "snip/init.hpp"
#include "snip/bpe.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: snip <command> [args...]\n";
        std::cerr << "       snip init [--claude]\n";
        std::cerr << "Example: snip git status\n";
        return 1;
    }

    std::string first_arg = argv[1];

    if (first_arg == "init") {
        bool for_claude = (argc > 2 && std::string(argv[2]) == "--claude");
        if (for_claude) {
            snip::SetupManager::setup_claude_code();
        }
        snip::SetupManager::setup_shims();
        return 0;
    }

    std::vector<std::string> cmd_args;
    cmd_args.reserve(static_cast<size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
        cmd_args.emplace_back(argv[i]);
    }

    // 1. Run command
    snip::CommandResult res = snip::ProcessRunner::execute(cmd_args);

    // 2. Sanitize ANSI & progress bars
    std::string clean_stdout = snip::StreamSanitizer::sanitize(res.stdout_output);
    std::string clean_stderr = snip::StreamSanitizer::sanitize(res.stderr_output);

    // 3. Domain Parsing
    snip::ParseResult parsed = snip::Dispatcher::route_and_parse(cmd_args, clean_stdout, res.exit_code);

    // 4. Output the result
    std::cout << parsed.text;
    if (!clean_stderr.empty()) {
        std::cerr << clean_stderr;
    }

    // 5. Accurate Token Counting with Embedded BPE
    if (parsed.was_compressed) {
        snip::BPETokenizer tokenizer;
        size_t orig_tokens = tokenizer.count_tokens(res.stdout_output);
        size_t compressed_tokens = tokenizer.count_tokens(parsed.text);

        if (orig_tokens > compressed_tokens) {
            snip::RecoveryCache::save_raw(res.stdout_output);

            size_t saved_tokens = orig_tokens - compressed_tokens;
            double saved_pct = (static_cast<double>(saved_tokens) / static_cast<double>(orig_tokens)) * 100.0;

            std::cout << "\n[snip: " << orig_tokens << " -> " << compressed_tokens 
                      << " tokens (" << saved_tokens << " saved, " 
                      << static_cast<int>(saved_pct) << "% reduction). Raw: " 
                      << snip::RecoveryCache::CACHE_PATH << "]\n";
        }
    }

    return res.exit_code;
}
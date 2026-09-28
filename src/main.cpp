#include "snip/runner.hpp"
#include "snip/dispatcher.hpp"
#include "snip/cache.hpp"
#include "snip/init.hpp"
#include "snip/bpe.hpp"
#include "snip/stats.hpp"
#include "snip/mcp_server.hpp"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: snip <command> [args...]\n"
                  << "       snip init [--claude|--shims]\n"
                  << "       snip gain\n";
        return 1;
    }

    std::string first_arg = argv[1];

    if (first_arg == "init") {
        if (argc > 2 && std::string(argv[2]) == "--claude") {
            return snip::SetupManager::setup_claude_code() ? 0 : 1;
        }
        return snip::SetupManager::setup_shims() ? 0 : 1;
    }
    // Start the MCP server
    if (first_arg == "mcp") {
        return snip::MCPServer::run();
    }

    if (first_arg == "gain" || first_arg == "stats") {
        snip::StatsManager::print_dashboard();
        return 0;
    }

    std::vector<std::string> cmd_args;
    for (int i = 1; i < argc; ++i) {
        cmd_args.emplace_back(argv[i]);
    }

    auto run_res = snip::ProcessRunner::execute(cmd_args);

    std::string raw_combined;
    if (!run_res.stdout_output.empty() && !run_res.stderr_output.empty()) {
        raw_combined = run_res.stdout_output + "\n" + run_res.stderr_output;
    } else if (!run_res.stdout_output.empty()) {
        raw_combined = run_res.stdout_output;
    } else {
        raw_combined = run_res.stderr_output;
    }

    auto parse_res = snip::Dispatcher::route_and_parse(
        cmd_args,
        run_res.stdout_output,
        run_res.stderr_output,
        run_res.exit_code
    );

    snip::BPETokenizer tokenizer;
    size_t orig_tokens = tokenizer.count_tokens(raw_combined);
    size_t comp_tokens = tokenizer.count_tokens(parse_res.text);

    snip::StatsManager::record_run(orig_tokens, comp_tokens);

    snip::RecoveryCache::save_raw(raw_combined);

    if (run_res.exit_code != 0 && run_res.stdout_output.empty()) {
        std::cerr << parse_res.text;
    } else {
        std::cout << parse_res.text;
    }

    if (parse_res.was_compressed) {
        size_t saved_tokens = (orig_tokens > comp_tokens) ? (orig_tokens - comp_tokens) : 0;
        int pct = (orig_tokens > 0) ? static_cast<int>((saved_tokens * 100) / orig_tokens) : 0;
        std::cout << "\n[snip: " << orig_tokens << " -> " << comp_tokens
                  << " tokens (" << saved_tokens << " saved, " << pct << "% reduction). Raw: "
                  << snip::RecoveryCache::CACHE_PATH << "]\n";
    }

    return run_res.exit_code;
}
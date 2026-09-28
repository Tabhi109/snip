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
                  << "       snip init [bash|zsh|--claude|--cursor|--shims]\n"
                  << "       snip gain\n"
                  << "       snip mcp\n";
        return 1;
    }

    std::string first_arg = argv[1];

    if (first_arg == "--help" || first_arg == "-h") {
        std::cout << "snip - Low-latency token pruning engine & MCP server for AI coding agents\n\n"
                  << "Usage:\n"
                  << "  snip <command> [args...]      Run command and prune verbose CLI output\n"
                  << "  snip init [target]            Configure shell integration or agent MCP\n"
                  << "  snip gain                     Show token ROI and financial savings ledger\n"
                  << "  snip mcp                      Start JSON-RPC 2.0 MCP server over stdio\n\n"
                  << "Init Targets:\n"
                  << "  snip init zsh                 Emit zsh alias wrappers for eval \"$(snip init zsh)\"\n"
                  << "  snip init bash                Emit bash alias wrappers for eval \"$(snip init bash)\"\n"
                  << "  snip init --claude            Register snip MCP server in Claude Desktop\n"
                  << "  snip init --cursor            Show Cursor MCP setup instructions\n"
                  << "  snip init --shims             Install transparent intercept shims in ~/.snip/bin\n";
        return 0;
    }

    if (first_arg == "--version" || first_arg == "-v") {
        std::cout << "snip version 0.2.0 (C++20)\n";
        return 0;
    }

    if (first_arg == "init") {
        if (argc > 2) {
            std::string sub = argv[2];
            if (sub == "bash" || sub == "zsh") {
                snip::SetupManager::emit_shell_aliases(sub);
                return 0;
            } else if (sub == "--claude") {
                return snip::SetupManager::setup_claude_desktop() ? 0 : 1;
            } else if (sub == "--cursor") {
                return snip::SetupManager::setup_cursor() ? 0 : 1;
            } else if (sub == "--shims") {
                return snip::SetupManager::setup_shims() ? 0 : 1;
            } else if (sub == "--claude-code") {
                return snip::SetupManager::setup_claude_code() ? 0 : 1;
            }
        }
        std::cout << "snip init: One-command shell & agent auto-config\n\n"
                  << "Usage:\n"
                  << "  snip init zsh        Emit zsh alias integration\n"
                  << "  snip init bash       Emit bash alias integration\n"
                  << "  snip init --claude   Auto-register in Claude Desktop config\n"
                  << "  snip init --cursor   Print Cursor MCP configuration instructions\n"
                  << "  snip init --shims    Install global shims in ~/.snip/bin\n";
        return 0;
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
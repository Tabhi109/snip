#include "snip/init.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace snip {

namespace {

std::string resolve_snip_binary() {
    const char* home = std::getenv("HOME");
    std::vector<fs::path> candidates;
    if (home) {
        candidates.push_back(fs::path(home) / ".local" / "bin" / "snip");
        candidates.push_back(fs::path(home) / ".snip" / "bin" / "snip");
    }
    candidates.push_back("/opt/homebrew/bin/snip");
    candidates.push_back("/usr/local/bin/snip");
    candidates.push_back("/usr/bin/snip");

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
    }
    return "snip";
}

} // namespace

void SetupManager::emit_shell_aliases(const std::string& shell_name) {
    std::string rc_file = (shell_name == "zsh") ? "~/.zshrc" : "~/.bashrc";
    std::cout << "# snip " << shell_name << " integration\n"
              << "# Add the following line to " << rc_file << " to activate automatically:\n"
              << "#   eval \"$(snip init " << shell_name << ")\"\n\n"
              << "alias git=\"snip git\"\n"
              << "alias docker=\"snip docker\"\n"
              << "alias kubectl=\"snip kubectl\"\n"
              << "alias pytest=\"snip pytest\"\n"
              << "alias cargo=\"snip cargo\"\n";
}

bool SetupManager::setup_claude_desktop() {
    const char* home = std::getenv("HOME");
    if (!home) {
        std::cerr << "snip: Could not locate $HOME directory.\n";
        return false;
    }

    // Determine standard platform config path
    fs::path config_dir;
#if defined(__APPLE__)
    config_dir = fs::path(home) / "Library" / "Application Support" / "Claude";
#elif defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    config_dir = appdata ? (fs::path(appdata) / "Claude") : (fs::path(home) / "AppData" / "Roaming" / "Claude");
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    config_dir = xdg ? (fs::path(xdg) / "Claude") : (fs::path(home) / ".config" / "Claude");
#endif

    fs::path config_file = config_dir / "claude_desktop_config.json";

    std::string snip_bin = resolve_snip_binary();

    const std::string sample_mcp_block =
        "{\n"
        "  \"mcpServers\": {\n"
        "    \"snip\": {\n"
        "      \"command\": \"" + snip_bin + "\",\n"
        "      \"args\": [\"mcp\"]\n"
        "    }\n"
        "  }\n"
        "}\n";

    try {
        if (!fs::exists(config_dir)) {
            fs::create_directories(config_dir);
        }

        if (fs::exists(config_file)) {
            std::ifstream in(config_file);
            std::stringstream buf;
            buf << in.rdbuf();
            std::string content = buf.str();
            in.close();

            if (content.find("\"snip\"") != std::string::npos) {
                std::cout << "✓ snip MCP server is already registered in Claude Desktop config:\n"
                          << "  " << config_file.string() << "\n";
                return true;
            }

            // Check if "mcpServers" key exists
            size_t mcp_pos = content.find("\"mcpServers\"");
            if (mcp_pos != std::string::npos) {
                size_t brace = content.find('{', mcp_pos);
                if (brace != std::string::npos) {
                    std::string addition = "\n    \"snip\": {\n"
                                           "      \"command\": \"" + snip_bin + "\",\n"
                                           "      \"args\": [\"mcp\"]\n"
                                           "    },";
                    content.insert(brace + 1, addition);
                    std::ofstream out(config_file, std::ios::trunc);
                    out << content;
                    out.close();

                    std::cout << "✓ Added 'snip' MCP server to existing Claude Desktop config:\n"
                              << "  " << config_file.string() << "\n";
                    return true;
                }
            }
        }

        // Write fresh config if file was absent or didn't contain mcpServers
        std::ofstream out(config_file, std::ios::trunc);
        out << sample_mcp_block;
        out.close();

        std::cout << "✓ Created Claude Desktop config with 'snip' MCP server:\n"
                  << "  " << config_file.string() << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "! Could not automatically update Claude Desktop config: " << e.what() << "\n\n";
        std::cout << "You can manually configure Claude Desktop by placing this in:\n"
                  << "  " << config_file.string() << "\n\n"
                  << sample_mcp_block << "\n";
        return false;
    }
}

bool SetupManager::setup_cursor() {
    std::string snip_bin = resolve_snip_binary();

    fs::path cursor_dir = ".cursor";
    fs::path cursor_mcp = cursor_dir / "mcp.json";
    fs::path cursor_rules_dir = cursor_dir / "rules";
    fs::path cursor_mdc = cursor_rules_dir / "snip.mdc";
    fs::path cursorrules_file = ".cursorrules";

    try {
        if (!fs::exists(cursor_dir)) {
            fs::create_directories(cursor_dir);
        }

        // 1. Setup .cursor/mcp.json
        if (!fs::exists(cursor_mcp)) {
            std::ofstream out(cursor_mcp, std::ios::trunc);
            out << "{\n"
                << "  \"mcpServers\": {\n"
                << "    \"snip\": {\n"
                << "      \"command\": \"" << snip_bin << "\",\n"
                << "      \"args\": [\"mcp\"]\n"
                << "    }\n"
                << "  }\n"
                << "}\n";
            out.close();
            std::cout << "✓ Created " << cursor_mcp.string() << " (command: " << snip_bin << ")\n";
        } else {
            std::ifstream in(cursor_mcp);
            std::stringstream buf;
            buf << in.rdbuf();
            std::string content = buf.str();
            in.close();

            if (content.find("\"snip\"") == std::string::npos) {
                size_t mcp_pos = content.find("\"mcpServers\"");
                if (mcp_pos != std::string::npos) {
                    size_t brace = content.find('{', mcp_pos);
                    if (brace != std::string::npos) {
                        std::string addition = "\n    \"snip\": {\n"
                                               "      \"command\": \"" + snip_bin + "\",\n"
                                               "      \"args\": [\"mcp\"]\n"
                                               "    },";
                        content.insert(brace + 1, addition);
                        std::ofstream out(cursor_mcp, std::ios::trunc);
                        out << content;
                        out.close();
                        std::cout << "✓ Added 'snip' MCP server to " << cursor_mcp.string() << "\n";
                    }
                }
            } else {
                std::cout << "✓ 'snip' MCP server already registered in " << cursor_mcp.string() << "\n";
            }
        }

        // 2. Setup Cursor Agent Rule so Cursor Agent actively calls snip_exec
        if (!fs::exists(cursor_rules_dir)) {
            fs::create_directories(cursor_rules_dir);
        }
        if (!fs::exists(cursor_mdc)) {
            std::ofstream out(cursor_mdc, std::ios::trunc);
            out << "---\n"
                << "description: Optimize CLI token consumption via snip MCP\n"
                << "globs: *\n"
                << "---\n"
                << "# snip Token Optimization\n\n"
                << "Always execute CLI and terminal commands using the `snip_exec` tool from the `snip` MCP server\n"
                << "instead of running commands directly through standard terminal execution. This reduces context\n"
                << "token bloat from build logs, test suites, and terminal output by 50% to 90%.\n";
            out.close();
            std::cout << "✓ Created agent rule in " << cursor_mdc.string() << "\n";
        }

        // Also create .cursorrules for legacy compatibility if absent
        if (!fs::exists(cursorrules_file)) {
            std::ofstream out(cursorrules_file, std::ios::trunc);
            out << "# snip Token Optimization\n"
                << "Always execute CLI and terminal commands using the `snip_exec` tool from the `snip` MCP server.\n";
            out.close();
            std::cout << "✓ Created " << cursorrules_file.string() << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "! Notice: Could not auto-generate .cursor workspace files: " << e.what() << "\n";
    }

    std::cout << "\n============================================================\n"
              << "               Cursor MCP Server Configuration              \n"
              << "============================================================\n\n"
              << "Workspace files configured:\n"
              << "  - .cursor/mcp.json        : Registers snip MCP server (" << snip_bin << ")\n"
              << "  - .cursor/rules/snip.mdc  : Instructs Cursor Agent to use snip_exec\n"
              << "  - .cursorrules            : Legacy agent fallback rule\n\n"
              << "Manual Setup (Cursor Settings UI):\n"
              << "  1. Open Cursor Settings (Cmd+, on macOS, Ctrl+, on Linux/Windows)\n"
              << "  2. Navigate to: Features > MCP Servers\n"
              << "  3. Click 'Add New MCP Server':\n"
              << "       Name    : snip\n"
              << "       Type    : command\n"
              << "       Command : " << snip_bin << " mcp\n\n"
              << "Why .cursorrules / snip.mdc is required:\n"
              << "  Cursor Agent defaults to its built-in terminal runner rather than\n"
              << "  MCP tools. The generated rule instructs the agent to route commands via snip_exec,\n"
              << "  allowing snip to prune outputs and record token ROI (visible in 'snip gain').\n\n"
              << "Tools exposed to Cursor Agent:\n"
              << "  - snip_exec      : Executes any CLI command with deterministic token pruning\n"
              << "  - snip_read_file : Reads file in structural skeleton mode\n"
              << "  - snip_search    : Fast ripgrep/grep search with capped hierarchical grouping\n\n";
    return true;
}

bool SetupManager::setup_shims() {
    const char* home = std::getenv("HOME");
    if (!home) {
        std::cerr << "snip: Could not locate $HOME directory.\n";
        return false;
    }

    fs::path snip_dir = fs::path(home) / ".snip" / "bin";

    try {
        fs::create_directories(snip_dir);
    } catch (const std::exception& e) {
        std::cerr << "snip: Failed to create " << snip_dir << ": " << e.what() << "\n";
        return false;
    }

    std::string snip_bin = resolve_snip_binary();

    // List of common noisy commands to shim
    const std::vector<std::string> shim_commands = {
        "git", "docker", "kubectl", "pytest", "cargo", "npm"
    };

    for (const auto& cmd : shim_commands) {
        fs::path shim_file = snip_dir / cmd;
        std::ofstream out(shim_file, std::ios::out | std::ios::trunc);
        if (!out.is_open()) continue;

        out << "#!/bin/sh\n";
        out << "exec " << snip_bin << " " << cmd << " \"$@\"\n";
        out.close();

        // Make executable (chmod 755)
        fs::permissions(shim_file, 
            fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec |
            fs::perms::group_read | fs::perms::group_exec |
            fs::perms::others_read | fs::perms::others_exec,
            fs::perm_options::replace);
    }

    std::cout << "✓ Created shims in " << snip_dir << "\n\n";
    std::cout << "To activate universally for all agents (Cursor, Claude Code, Aider):\n";
    std::cout << "Add this to your ~/.zshrc or ~/.bashrc:\n\n";
    std::cout << "    export PATH=\"" << snip_dir.string() << ":$PATH\"\n\n";

    return true;
}

bool SetupManager::setup_claude_code() {
    const char* home = std::getenv("HOME");
    if (!home) return false;

    fs::path claude_config_dir = fs::path(home) / ".claude";
    fs::path hook_script = claude_config_dir / "snip_hook.sh";

    try {
        fs::create_directories(claude_config_dir);
    } catch (...) {
        return false;
    }

    std::string snip_bin = resolve_snip_binary();

    std::ofstream out(hook_script, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;

    out << "#!/bin/sh\n";
    out << "# Hook for Claude Code PreToolUse\n";
    out << "CMD=\"$1\"\n";
    out << "case \"$CMD\" in\n";
    out << "  git*|docker*|kubectl*|pytest*|cargo*|npm*)\n";
    out << "    exec " << snip_bin << " $CMD ;;\n";
    out << "  *)\n";
    out << "    exec $CMD ;;\n";
    out << "esac\n";
    out.close();

    fs::permissions(hook_script, fs::perms::all, fs::perm_options::replace);

    std::cout << "✓ Claude Code hook script generated at: " << hook_script << "\n";
    return true;
}

} // namespace snip

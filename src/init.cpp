#include "snip/init.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace snip {

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

    std::string snip_bin = "snip";
    // Check if installed in standard locations or current path
    if (fs::exists("/usr/local/bin/snip")) {
        snip_bin = "/usr/local/bin/snip";
    } else if (fs::exists(fs::path(home) / ".snip" / "bin" / "snip")) {
        snip_bin = (fs::path(home) / ".snip" / "bin" / "snip").string();
    }

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
    std::string snip_bin = "snip";
    const char* home = std::getenv("HOME");
    if (home) {
        if (fs::exists("/usr/local/bin/snip")) {
            snip_bin = "/usr/local/bin/snip";
        } else if (fs::exists(fs::path(home) / ".snip" / "bin" / "snip")) {
            snip_bin = (fs::path(home) / ".snip" / "bin" / "snip").string();
        }
    }

    std::cout << "\n============================================================\n"
              << "               Cursor MCP Server Configuration              \n"
              << "============================================================\n\n"
              << "Option 1: Add via Cursor Settings UI\n"
              << "  1. Open Cursor Settings (Cmd+, on macOS, Ctrl+, on Linux/Windows)\n"
              << "  2. Navigate to: Features > MCP Servers\n"
              << "  3. Click 'Add New MCP Server':\n"
              << "       Name    : snip\n"
              << "       Type    : command\n"
              << "       Command : " << snip_bin << " mcp\n\n"
              << "Option 2: Workspace Project Configuration (.cursor/mcp.json)\n"
              << "  Create or update .cursor/mcp.json in your project repository:\n\n"
              << "{\n"
              << "  \"mcpServers\": {\n"
              << "    \"snip\": {\n"
              << "      \"command\": \"" << snip_bin << "\",\n"
              << "      \"args\": [\"mcp\"]\n"
              << "    }\n"
              << "  }\n"
              << "}\n\n"
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

    // List of common noisy commands to shim
    const std::vector<std::string> shim_commands = {
        "git", "docker", "kubectl", "pytest", "cargo", "npm"
    };

    for (const auto& cmd : shim_commands) {
        fs::path shim_file = snip_dir / cmd;
        std::ofstream out(shim_file, std::ios::out | std::ios::trunc);
        if (!out.is_open()) continue;

        out << "#!/bin/sh\n";
        out << "exec snip " << cmd << " \"$@\"\n";
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

    std::ofstream out(hook_script, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;

    out << "#!/bin/sh\n";
    out << "# Hook for Claude Code PreToolUse\n";
    out << "CMD=\"$1\"\n";
    out << "case \"$CMD\" in\n";
    out << "  git*|docker*|kubectl*|pytest*|cargo*|npm*)\n";
    out << "    exec snip $CMD ;;\n";
    out << "  *)\n";
    out << "    exec $CMD ;;\n";
    out << "esac\n";
    out.close();

    fs::permissions(hook_script, fs::perms::all, fs::perm_options::replace);

    std::cout << "✓ Claude Code hook script generated at: " << hook_script << "\n";
    return true;
}

} // namespace snip
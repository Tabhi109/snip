#include "snip/init.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

namespace snip {

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
        "git", "pytest", "cargo", "npm"
    };

    // Find absolute path of the current snip executable
    // (In production, this would be /usr/local/bin/snip)
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
    std::cout << "To activate universally for all agents (Cursor, Aider, subshells):\n";
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
    out << "  git*|pytest*|cargo*|npm*)\n";
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
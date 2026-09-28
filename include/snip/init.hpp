#pragma once

#include <string>

namespace snip {

class SetupManager {
public:
    // Emits shell aliases for bash or zsh (eval "$(snip init zsh)")
    static void emit_shell_aliases(const std::string& shell_name);

    // Configures Claude Desktop MCP integration safely
    static bool setup_claude_desktop();

    // Prints step-by-step instructions and JSON block for Cursor MCP
    static bool setup_cursor();

    // Installs shims into ~/.snip/bin and prints shell export instructions
    static bool setup_shims();

    // Sets up Claude Code hook integration
    static bool setup_claude_code();
};

} // namespace snip
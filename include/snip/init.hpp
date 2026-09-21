#pragma once

#include <string>

namespace snip {

class SetupManager {
public:
    // Installs shims into ~/.snip/bin and prints shell export instructions
    static bool setup_shims();

    // Sets up Claude Code hook integration
    static bool setup_claude_code();
};

} // namespace snip
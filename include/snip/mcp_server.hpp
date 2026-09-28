#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace snip {

class MCPServer {
public:
    // Starts the stdio JSON-RPC 2.0 loop until EOF
    static int run();

    // Parse command line into discrete arguments respecting quotes, escaping, and spaces
    static std::vector<std::string> parse_command_line(std::string_view cmd);

private:
    static void handle_request(std::string_view line);
    static void send_response(const std::string& json_str);
};

} // namespace snip
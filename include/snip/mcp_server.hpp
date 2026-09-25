#pragma once

#include <string>
#include <string_view>

namespace snip {

class MCPServer {
public:
    // Starts the stdio JSON-RPC 2.0 loop until EOF
    static int run();

private:
    static void handle_request(std::string_view line);
    static void send_response(const std::string& json_str);
};

} // namespace snip
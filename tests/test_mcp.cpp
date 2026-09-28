#include <catch2/catch_test_macros.hpp>
#include "snip/mcp_server.hpp"

TEST_CASE("MCPServer parse_command_line handles simple unquoted commands", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("git status --short");
    REQUIRE(args.size() == 3);
    CHECK(args[0] == "git");
    CHECK(args[1] == "status");
    CHECK(args[2] == "--short");
}

TEST_CASE("MCPServer parse_command_line handles double quotes with spaces", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("git commit -m \"feat: test message\"");
    REQUIRE(args.size() == 4);
    CHECK(args[0] == "git");
    CHECK(args[1] == "commit");
    CHECK(args[2] == "-m");
    CHECK(args[3] == "feat: test message");
}

TEST_CASE("MCPServer parse_command_line handles single quotes with nested content", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("sh -c 'echo \"inner string\" && exit 0'");
    REQUIRE(args.size() == 3);
    CHECK(args[0] == "sh");
    CHECK(args[1] == "-c");
    CHECK(args[2] == "echo \"inner string\" && exit 0");
}

TEST_CASE("MCPServer parse_command_line handles escaped quotes and backslashes", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("echo \"He said: \\\"Hello\\\" and \\\\\"");
    REQUIRE(args.size() == 2);
    CHECK(args[0] == "echo");
    CHECK(args[1] == "He said: \"Hello\" and \\");
}

TEST_CASE("MCPServer parse_command_line handles unquoted backslash escapes and spaces", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("cat path\\ with\\ spaces/file.txt");
    REQUIRE(args.size() == 2);
    CHECK(args[0] == "cat");
    CHECK(args[1] == "path with spaces/file.txt");
}

TEST_CASE("MCPServer parse_command_line handles adjacent quoted tokens", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("--flag=\"first part\"' second part'");
    REQUIRE(args.size() == 1);
    CHECK(args[0] == "--flag=first part second part");
}

TEST_CASE("MCPServer parse_command_line handles empty quoted arguments", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("run \"\" '' arg");
    REQUIRE(args.size() == 4);
    CHECK(args[0] == "run");
    CHECK(args[1] == "");
    CHECK(args[2] == "");
    CHECK(args[3] == "arg");
}

TEST_CASE("MCPServer parse_command_line handles whitespace variations", "[mcp]") {
    auto args = snip::MCPServer::parse_command_line("   cmd   arg1 \t  arg2\n  arg3   ");
    REQUIRE(args.size() == 4);
    CHECK(args[0] == "cmd");
    CHECK(args[1] == "arg1");
    CHECK(args[2] == "arg2");
    CHECK(args[3] == "arg3");

    auto empty_args = snip::MCPServer::parse_command_line("   \t  \n  ");
    CHECK(empty_args.empty());
}

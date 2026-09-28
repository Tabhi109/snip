#include "snip/mcp_server.hpp"
#include "snip/runner.hpp"
#include "snip/dispatcher.hpp"
#include "snip/cache.hpp"
#include "snip/bpe.hpp"
#include "snip/stats.hpp"
#include "snip/file_parser.hpp"
#include "snip/search_parser.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>

namespace snip {

namespace {

std::string json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) >= 0x20) {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string extract_json_string(std::string_view json, std::string_view key) {
    std::string needle = "\"" + std::string(key) + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";

    size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return "";

    size_t first_quote = json.find('"', colon + 1);
    if (first_quote == std::string::npos) return "";

    size_t second_quote = first_quote + 1;
    while (second_quote < json.size()) {
        if (json[second_quote] == '"') {
            size_t b = second_quote;
            while (b > first_quote && json[b - 1] == '\\') {
                b--;
            }
            if ((second_quote - b) % 2 == 0) {
                break;
            }
        }
        second_quote++;
    }
    if (second_quote >= json.size()) return "";

    std::string_view raw = json.substr(first_quote + 1, second_quote - first_quote - 1);
    std::string unescaped;
    unescaped.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            char next = raw[i + 1];
            if (next == '"') { unescaped += '"'; ++i; }
            else if (next == '\\') { unescaped += '\\'; ++i; }
            else if (next == '/') { unescaped += '/'; ++i; }
            else if (next == 'b') { unescaped += '\b'; ++i; }
            else if (next == 'f') { unescaped += '\f'; ++i; }
            else if (next == 'n') { unescaped += '\n'; ++i; }
            else if (next == 'r') { unescaped += '\r'; ++i; }
            else if (next == 't') { unescaped += '\t'; ++i; }
            else { unescaped += raw[i]; }
        } else {
            unescaped += raw[i];
        }
    }
    return unescaped;
}

std::string extract_json_id(std::string_view json) {
    size_t id_pos = json.find("\"id\":");
    if (id_pos == std::string::npos) {
        id_pos = json.find("\"id\" :");
        if (id_pos == std::string::npos) return "null";
    }

    size_t colon = json.find(':', id_pos);
    if (colon == std::string::npos) return "null";

    size_t start = json.find_first_not_of(" \t\r\n", colon + 1);
    if (start == std::string::npos) return "null";

    // If ID is a string: "id": "req-1"
    if (json[start] == '"') {
        size_t end_quote = json.find('"', start + 1);
        while (end_quote != std::string_view::npos && json[end_quote - 1] == '\\') {
            end_quote = json.find('"', end_quote + 1);
        }
        if (end_quote != std::string_view::npos) {
            return std::string(json.substr(start, end_quote - start + 1));
        }
        return "null";
    }

    // If ID is an integer or raw literal: "id": 2
    size_t end = json.find_first_of(",}\r\n ", start);
    if (end != std::string_view::npos) {
        return std::string(json.substr(start, end - start));
    }
    return std::string(json.substr(start));
}

} // namespace

std::vector<std::string> MCPServer::parse_command_line(std::string_view cmd) {
    std::vector<std::string> args;
    std::string current;
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool has_token = false;

    for (size_t i = 0; i < cmd.size(); ++i) {
        char c = cmd[i];

        if (in_single_quote) {
            if (c == '\'') {
                in_single_quote = false;
            } else {
                current += c;
            }
        } else if (in_double_quote) {
            if (c == '\\') {
                if (i + 1 < cmd.size()) {
                    char next = cmd[i + 1];
                    if (next == '"' || next == '\\' || next == '$' || next == '`') {
                        current += next;
                        ++i;
                    } else {
                        current += '\\';
                    }
                } else {
                    current += '\\';
                }
            } else if (c == '"') {
                in_double_quote = false;
            } else {
                current += c;
            }
        } else {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                if (has_token) {
                    args.push_back(std::move(current));
                    current.clear();
                    has_token = false;
                }
            } else if (c == '\'') {
                in_single_quote = true;
                has_token = true;
            } else if (c == '"') {
                in_double_quote = true;
                has_token = true;
            } else if (c == '\\') {
                has_token = true;
                if (i + 1 < cmd.size()) {
                    current += cmd[++i];
                } else {
                    current += '\\';
                }
            } else {
                current += c;
                has_token = true;
            }
        }
    }

    if (has_token) {
        args.push_back(std::move(current));
    }

    return args;
}

void MCPServer::send_response(const std::string& json_str) {
    std::cout << json_str << "\n" << std::flush;
}

void MCPServer::handle_request(std::string_view line) {
    if (line.empty()) return;

    std::string method = extract_json_string(line, "method");
    std::string id = extract_json_id(line);

    // 1. Initialize
    if (method == "initialize") {
#ifndef SNIP_VERSION
#define SNIP_VERSION "0.0.0"
#endif
        std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
            ",\"result\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},"
            "\"serverInfo\":{\"name\":\"snip\",\"version\":\"" + std::string(SNIP_VERSION) + "\"}}}";
        send_response(resp);
        return;
    }

    // 2. Initialized notification
    if (method == "notifications/initialized") {
        return;
    }

    // 3. List Tools
    if (method == "tools/list") {
        std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":{\"tools\":["
            "{"
                "\"name\":\"snip_exec\","
                "\"description\":\"Executes a CLI command and returns deterministic token-pruned output with BPE metrics.\","
                "\"inputSchema\":{"
                    "\"type\":\"object\","
                    "\"properties\":{\"command\":{\"type\":\"string\",\"description\":\"The shell command to execute\"}},"
                    "\"required\":[\"command\"]"
                "}"
            "},"
            "{"
                "\"name\":\"snip_read_file\","
                "\"description\":\"Reads a code file in structural skeleton mode (extracts signatures, collapses bodies).\","
                "\"inputSchema\":{"
                    "\"type\":\"object\","
                    "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Path to the file to inspect\"}},"
                    "\"required\":[\"path\"]"
                "}"
            "},"
            "{"
                "\"name\":\"snip_search\","
                "\"description\":\"Performs fast token-pruned codebase search with hierarchical grouping and match capping.\","
                "\"inputSchema\":{"
                    "\"type\":\"object\","
                    "\"properties\":{"
                        "\"query\":{\"type\":\"string\",\"description\":\"Search query or regex pattern\"},"
                        "\"path\":{\"type\":\"string\",\"description\":\"Directory or file to search (defaults to .)\"}"
                    "},"
                    "\"required\":[\"query\"]"
                "}"
            "}"
        "]}}";
        send_response(resp);
        return;
    }

    // 4. Call Tool
    if (method == "tools/call") {
        std::string tool_name = extract_json_string(line, "name");

        if (tool_name == "snip_exec") {
            std::string cmd = extract_json_string(line, "command");
            std::vector<std::string> args = parse_command_line(cmd);

            auto run_res = ProcessRunner::execute(args);
            auto parse_res = Dispatcher::route_and_parse(args, run_res.stdout_output, run_res.exit_code);

            BPETokenizer tokenizer;
            size_t orig_tokens = tokenizer.count_tokens(run_res.stdout_output);
            size_t comp_tokens = tokenizer.count_tokens(parse_res.text);
            StatsManager::record_run(orig_tokens, comp_tokens);
            RecoveryCache::save_raw(run_res.stdout_output);

            std::string final_text = parse_res.text;
            if (parse_res.was_compressed) {
                size_t saved = (orig_tokens > comp_tokens) ? (orig_tokens - comp_tokens) : 0;
                int pct = (orig_tokens > 0) ? static_cast<int>((saved * 100) / orig_tokens) : 0;
                final_text += "\n[snip: " + std::to_string(orig_tokens) + " -> " + 
                              std::to_string(comp_tokens) + " tokens (" + std::to_string(saved) + 
                              " saved, " + std::to_string(pct) + "% reduction)]";
            }

            std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
                ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + 
                json_escape(final_text) + "\"}]}}";
            send_response(resp);
            return;
        }

        if (tool_name == "snip_read_file") {
            std::string path = extract_json_string(line, "path");
            std::ifstream file(path);
            if (!file.is_open()) {
                std::string err = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
                    ",\"result\":{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"Failed to open file: " + 
                    json_escape(path) + "\"}]}}";
                send_response(err);
                return;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            std::string skeleton = FileParser::skeletonize_code(content, "");

            BPETokenizer tokenizer;
            size_t orig_tokens = tokenizer.count_tokens(content);
            size_t comp_tokens = tokenizer.count_tokens(skeleton);
            StatsManager::record_run(orig_tokens, comp_tokens);

            skeleton += "\n[snip skeleton: " + std::to_string(orig_tokens) + " -> " + 
                        std::to_string(comp_tokens) + " tokens]";

            std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
                ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + 
                json_escape(skeleton) + "\"}]}}";
            send_response(resp);
            return;
        }

        if (tool_name == "snip_search") {
            std::string query = extract_json_string(line, "query");
            std::string search_path = extract_json_string(line, "path");
            if (search_path.empty()) search_path = ".";

            if (query.empty()) {
                std::string err = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
                    ",\"result\":{\"isError\":true,\"content\":[{\"type\":\"text\",\"text\":\"'query' parameter is required\"}]}}";
                send_response(err);
                return;
            }

            // Execute ripgrep if available, otherwise grep
            std::vector<std::string> args = {"rg", "-n", "-H", "--no-heading", query, search_path};
            auto run_res = ProcessRunner::execute(args);
            if (run_res.exit_code == 127 || (run_res.stdout_output.empty() && run_res.stderr_output.find("not found") != std::string::npos)) {
                args = {"grep", "-rn", "-H", query, search_path};
                run_res = ProcessRunner::execute(args);
            }

            std::string pruned = SearchParser::parse_ripgrep(run_res.stdout_output);

            BPETokenizer tokenizer;
            size_t orig_tokens = tokenizer.count_tokens(run_res.stdout_output);
            size_t comp_tokens = tokenizer.count_tokens(pruned);
            StatsManager::record_run(orig_tokens, comp_tokens);

            if (run_res.stdout_output.size() > pruned.size()) {
                size_t saved = (orig_tokens > comp_tokens) ? (orig_tokens - comp_tokens) : 0;
                int pct = (orig_tokens > 0) ? static_cast<int>((saved * 100) / orig_tokens) : 0;
                pruned += "\n[snip search: " + std::to_string(orig_tokens) + " -> " + 
                          std::to_string(comp_tokens) + " tokens (" + std::to_string(saved) + 
                          " saved, " + std::to_string(pct) + "% reduction)]";
            }

            std::string resp = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
                ",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"" + 
                json_escape(pruned) + "\"}]}}";
            send_response(resp);
            return;
        }

        std::string err = "{\"jsonrpc\":\"2.0\",\"id\":" + id + 
            ",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}";
        send_response(err);
        return;
    }

    if (method == "ping") {
        send_response("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":{}}");
        return;
    }
}

int MCPServer::run() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            handle_request(line);
        }
    }
    return 0;
}

} // namespace snip
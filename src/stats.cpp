#include "snip/stats.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

namespace snip {

std::string StatsManager::get_stats_file_path() {
    const char* home = std::getenv("HOME");
    std::string base = home ? std::string(home) : ".";
    fs::path dir = fs::path(base) / ".snip";
    if (!fs::exists(dir)) {
        fs::create_directories(dir);
    }
    return (dir / "stats.json").string();
}

StatsData StatsManager::load_stats() {
    StatsData data;
    std::string path = get_stats_file_path();
    std::ifstream in(path);
    if (!in.is_open()) return data;

    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"total_runs\":") != std::string::npos) {
            size_t colon = line.find(':');
            size_t comma = line.find(',', colon);
            std::string val = line.substr(colon + 1, (comma == std::string::npos ? line.size() : comma) - colon - 1);
            try { data.total_runs = std::stoull(val); } catch (...) {}
        } else if (line.find("\"original_tokens\":") != std::string::npos) {
            size_t colon = line.find(':');
            size_t comma = line.find(',', colon);
            std::string val = line.substr(colon + 1, (comma == std::string::npos ? line.size() : comma) - colon - 1);
            try { data.total_original_tokens = std::stoull(val); } catch (...) {}
        } else if (line.find("\"compressed_tokens\":") != std::string::npos) {
            size_t colon = line.find(':');
            size_t comma = line.find(',', colon);
            std::string val = line.substr(colon + 1, (comma == std::string::npos ? line.size() : comma) - colon - 1);
            try { data.total_compressed_tokens = std::stoull(val); } catch (...) {}
        }
    }
    return data;
}

void StatsManager::record_run(uint64_t original_tokens, uint64_t compressed_tokens) {
    StatsData data = load_stats();
    data.total_runs += 1;
    data.total_original_tokens += original_tokens;
    data.total_compressed_tokens += compressed_tokens;

    std::string path = get_stats_file_path();
    std::ofstream out(path, std::ios::trunc);
    if (out.is_open()) {
        out << "{\n";
        out << "  \"total_runs\": " << data.total_runs << ",\n";
        out << "  \"original_tokens\": " << data.total_original_tokens << ",\n";
        out << "  \"compressed_tokens\": " << data.total_compressed_tokens << "\n";
        out << "}\n";
    }
}

double StatsManager::calculate_savings_usd(uint64_t tokens_saved) noexcept {
    return static_cast<double>(tokens_saved) * BLENDED_COST_PER_TOKEN;
}

namespace {

std::string format_commas(uint64_t n) {
    std::string s = std::to_string(n);
    int insert_pos = static_cast<int>(s.length()) - 3;
    while (insert_pos > 0) {
        s.insert(static_cast<size_t>(insert_pos), ",");
        insert_pos -= 3;
    }
    return s;
}

void print_card_line(const std::string& text, size_t inner_width = 59) {
    std::cout << "│ ";
    if (text.size() > inner_width) {
        std::cout << text.substr(0, inner_width - 3) << "...";
    } else {
        std::cout << text << std::string(inner_width - text.size(), ' ');
    }
    std::cout << " │\n";
}

} // namespace

void StatsManager::print_dashboard() {
    StatsData data = load_stats();
    uint64_t saved = (data.total_original_tokens > data.total_compressed_tokens) 
                     ? (data.total_original_tokens - data.total_compressed_tokens) 
                     : 0;

    double pct = (data.total_original_tokens > 0)
                 ? (static_cast<double>(saved) / static_cast<double>(data.total_original_tokens)) * 100.0
                 : 0.0;
    double est_cost = calculate_savings_usd(saved);

    std::ostringstream pct_ss;
    pct_ss << std::fixed << std::setprecision(1) << pct << "% reduction";

    std::ostringstream cost_ss;
    cost_ss << "$" << std::fixed << std::setprecision(4) << est_cost << " USD";

    std::cout << "\n";
    // Top border ┌───┐
    std::cout << "┌";
    for (size_t i = 0; i < 61; ++i) std::cout << "\u2500";
    std::cout << "┐\n";

    // Centered Title
    std::string title = "snip Token ROI Ledger";
    size_t pad_left = (59 - title.size()) / 2;
    std::string centered_title = std::string(pad_left, ' ') + title;
    print_card_line(centered_title, 59);

    // Separator ├───┤
    std::cout << "├";
    for (size_t i = 0; i < 61; ++i) std::cout << "\u2500";
    std::cout << "┤\n";

    // Data rows
    print_card_line(" Commands Intercepted : " + format_commas(data.total_runs), 59);
    print_card_line(" Raw Input Tokens     : " + format_commas(data.total_original_tokens), 59);
    print_card_line(" Pruned Output Tokens : " + format_commas(data.total_compressed_tokens), 59);
    print_card_line(" Net Tokens Saved     : " + format_commas(saved) + " (" + pct_ss.str() + ")", 59);
    print_card_line(" Estimated Cost Saved : " + cost_ss.str(), 59);

    // Separator ├───┤
    std::cout << "├";
    for (size_t i = 0; i < 61; ++i) std::cout << "\u2500";
    std::cout << "┤\n";

    // Benchmark and Path Info
    print_card_line(" Benchmark : $3.00 / 1M tokens (Claude 3.5 Sonnet / GPT-4o)", 59);
    print_card_line(" Ledger    : " + get_stats_file_path(), 59);

    // Bottom border └───┘
    std::cout << "└";
    for (size_t i = 0; i < 61; ++i) std::cout << "\u2500";
    std::cout << "┘\n\n";
}

} // namespace snip
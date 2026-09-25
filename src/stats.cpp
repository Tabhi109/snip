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

void StatsManager::print_dashboard() {
    StatsData data = load_stats();
    uint64_t saved = (data.total_original_tokens > data.total_compressed_tokens) 
                     ? (data.total_original_tokens - data.total_compressed_tokens) 
                     : 0;

    double pct = (data.total_original_tokens > 0)
                 ? (static_cast<double>(saved) / static_cast<double>(data.total_original_tokens)) * 100.0
                 : 0.0;
    // Pricing benchmark: $3.00 per 1M tokens (Claude 3.5 Sonnet / GPT-4o input cost)
    double est_cost = (static_cast<double>(saved) / 1'000'000.0) * 3.00;

    std::cout << "\n=========================================\n";
    std::cout << "          snip Token ROI Ledger          \n";
    std::cout << "=========================================\n";
    std::cout << " Interceptions  : " << data.total_runs << " commands\n";
    std::cout << " Raw Tokens     : " << data.total_original_tokens << "\n";
    std::cout << " Sent Tokens    : " << data.total_compressed_tokens << "\n";
    std::cout << " Tokens Saved   : " << saved << " (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    std::cout << " Est. Cost Saved: $" << std::fixed << std::setprecision(4) << est_cost << " USD\n";
    std::cout << " Ledger Location: " << get_stats_file_path() << "\n";
    std::cout << "=========================================\n\n";
}

} // namespace snip
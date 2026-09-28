#pragma once

#include <string>
#include <cstdint>

namespace snip {

struct StatsData {
    uint64_t total_runs{0};
    uint64_t total_original_tokens{0};
    uint64_t total_compressed_tokens{0};
};

class StatsManager {
public:
    // Frontier model blended input pricing ($3.00 per 1M tokens)
    static constexpr double BLENDED_COST_PER_TOKEN = 3.00 / 1000000.0;

    static std::string get_stats_file_path();
    static StatsData load_stats();
    static void record_run(uint64_t original_tokens, uint64_t compressed_tokens);
    static double calculate_savings_usd(uint64_t tokens_saved) noexcept;
    static void print_dashboard();
};

} // namespace snip
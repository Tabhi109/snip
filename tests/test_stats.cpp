#include <catch2/catch_test_macros.hpp>
#include "snip/stats.hpp"
#include "snip/init.hpp"
#include <sstream>
#include <iostream>

TEST_CASE("StatsManager computes dollar ROI at $3 per 1M tokens", "[stats]") {
    REQUIRE(snip::StatsManager::calculate_savings_usd(0) == 0.0);
    
    // 1,000,000 tokens saved = $3.00
    double million_saved = snip::StatsManager::calculate_savings_usd(1'000'000);
    REQUIRE(million_saved == 3.00);

    // 500,000 tokens saved = $1.50
    double half_million = snip::StatsManager::calculate_savings_usd(500'000);
    REQUIRE(half_million == 1.50);

    // 10,000 tokens saved = $0.03
    double ten_thousand = snip::StatsManager::calculate_savings_usd(10'000);
    REQUIRE(std::abs(ten_thousand - 0.03) < 1e-6);
}

TEST_CASE("SetupManager emits shell alias wrappers for zsh and bash", "[init]") {
    // Capture stdout
    std::streambuf* orig_buf = std::cout.rdbuf();

    // 1. Zsh
    std::stringstream zsh_ss;
    std::cout.rdbuf(zsh_ss.rdbuf());
    snip::SetupManager::emit_shell_aliases("zsh");
    std::string zsh_out = zsh_ss.str();

    REQUIRE(zsh_out.find("eval \"$(snip init zsh)\"") != std::string::npos);
    REQUIRE(zsh_out.find("alias git=\"snip git\"") != std::string::npos);
    REQUIRE(zsh_out.find("alias docker=\"snip docker\"") != std::string::npos);
    REQUIRE(zsh_out.find("alias kubectl=\"snip kubectl\"") != std::string::npos);
    REQUIRE(zsh_out.find("alias pytest=\"snip pytest\"") != std::string::npos);

    // 2. Bash
    std::stringstream bash_ss;
    std::cout.rdbuf(bash_ss.rdbuf());
    snip::SetupManager::emit_shell_aliases("bash");
    std::string bash_out = bash_ss.str();

    REQUIRE(bash_out.find("eval \"$(snip init bash)\"") != std::string::npos);
    REQUIRE(bash_out.find("alias git=\"snip git\"") != std::string::npos);

    // Restore stdout
    std::cout.rdbuf(orig_buf);
}

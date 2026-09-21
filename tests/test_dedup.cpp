#include <catch2/catch_test_macros.hpp>
#include "snip/dedup.hpp"

TEST_CASE("Deduplicator collapses identical consecutive lines", "[dedup]") {
    std::string raw = 
        "polling service...\n"
        "polling service...\n"
        "polling service...\n"
        "connected!\n";

    std::string clean = snip::Deduplicator::collapse_repeated_lines(raw);
    REQUIRE(clean == "polling service... [repeated x3]\nconnected!\n");
}

TEST_CASE("Deduplicator preserves non-consecutive duplicates", "[dedup]") {
    std::string raw = 
        "ready\n"
        "working\n"
        "ready\n";

    std::string clean = snip::Deduplicator::collapse_repeated_lines(raw);
    REQUIRE(clean == "ready\nworking\nready\n");
}
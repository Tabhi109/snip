#include <catch2/catch_test_macros.hpp>
#include "snip/bpe.hpp"

TEST_CASE("BPETokenizer counts single characters and words", "[bpe]") {
    snip::BPETokenizer tokenizer;

    REQUIRE(tokenizer.count_tokens("") == 0);
    REQUIRE(tokenizer.count_tokens("a") == 1);
    
    // "git " should merge into fewer tokens than raw characters
    size_t tokens = tokenizer.count_tokens("git status");
    REQUIRE(tokens < 10);
}

TEST_CASE("BPETokenizer collapses multiple spaces into indentation tokens", "[bpe]") {
    snip::BPETokenizer tokenizer;

    // 4 spaces should count as 1 or 2 tokens, not 4
    size_t space_tokens = tokenizer.count_tokens("    ");
    REQUIRE(space_tokens <= 2);
}
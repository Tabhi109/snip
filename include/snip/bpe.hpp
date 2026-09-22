#pragma once

#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

namespace snip {

class BPETokenizer {
public:
    BPETokenizer();

    // Counts exact tokens for a given string_view
    size_t count_tokens(std::string_view text) const;

private:
    // Helper to run greedy BPE merges over a single word/slice
    size_t count_word_tokens(std::string_view piece) const;

    // Fast heuristic to split text into word-like chunks without pulling in huge regex engines
    static std::vector<std::string_view> split_into_chunks(std::string_view text);

    // Map storing the merge priority rank of byte pairs
    // Key: combined 64-bit representation of two 32-bit token IDs
    std::unordered_map<uint64_t, int32_t> merge_ranks_;

    static uint64_t make_pair_key(uint32_t a, uint32_t b) {
        return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
    }
};

} // namespace snip
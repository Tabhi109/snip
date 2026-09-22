#include "snip/bpe.hpp"
#include <algorithm>
#include <cctype>

namespace snip {

BPETokenizer::BPETokenizer() {
    int rank = 0;
    auto add_rule = [this, &rank](uint32_t a, uint32_t b) {
        merge_ranks_[make_pair_key(a, b)] = rank++;
    };

    // Common whitespace & indentations
    add_rule(' ', ' ');       // "  "
    add_rule(256, ' ');       // "   "
    add_rule(256, 256);       // "    "
    add_rule('\n', '\n');

    // High-frequency subwords & common programming terms
    add_rule('i', 'n'); add_rule('t', 'e'); add_rule('e', 'r');
    add_rule('o', 'n'); add_rule('a', 't'); add_rule('e', 's');
    add_rule('s', 't'); add_rule('o', 'r'); add_rule('a', 'r');
    add_rule('c', 'o'); add_rule('m', 'p'); add_rule('f', 'a');
    add_rule('i', 'l'); add_rule('e', 'd'); add_rule('p', 'a');
    add_rule('s', 's'); add_rule('g', 'i'); add_rule('t', ' ');
    add_rule('/', ' '); add_rule('-', '-'); add_rule(':', ' ');

    // Path tokens & extensions
    add_rule('.', 'c'); add_rule('c', 'p'); add_rule('p', 'p'); // .cpp
    add_rule('.', 'h'); add_rule('h', 'p');                     // .hpp
    add_rule('.', 't'); add_rule('t', 'x');                     // .txt
    add_rule('.', 'm'); add_rule('m', 'd');                     // .md
    add_rule('s', 'r'); add_rule('r', 'c');                     // src
    add_rule('t', 'e'); add_rule('s', 't');                     // test
    add_rule('M', ' '); add_rule('?', ' ');                     // Status markers
}

std::vector<std::string_view> BPETokenizer::split_into_chunks(std::string_view text) {
    std::vector<std::string_view> chunks;
    if (text.empty()) return chunks;

    size_t start = 0;
    while (start < text.size()) {
        // 1. Whitespace runs (spaces, tabs)
        if (text[start] == ' ' || text[start] == '\t') {
            size_t end = start + 1;
            while (end < text.size() && (text[end] == ' ' || text[end] == '\t')) {
                end++;
            }
            chunks.push_back(text.substr(start, end - start));
            start = end;
            continue;
        }

        // 2. Newlines stay isolated
        if (text[start] == '\n' || text[start] == '\r') {
            chunks.push_back(text.substr(start, 1));
            start++;
            continue;
        }

        // 3. Word characters and paths (alphanumeric + common path symbols)
        auto is_path_char = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || 
                   c == '/' || c == '.' || c == '_' || c == '-';
        };

        if (is_path_char(text[start])) {
            size_t end = start + 1;
            while (end < text.size() && is_path_char(text[end])) {
                end++;
            }
            chunks.push_back(text.substr(start, end - start));
            start = end;
            continue;
        }

        // 4. Standalone symbols
        chunks.push_back(text.substr(start, 1));
        start++;
    }

    return chunks;
}

size_t BPETokenizer::count_word_tokens(std::string_view piece) const {
    if (piece.empty()) return 0;
    if (piece.size() == 1) return 1;

    // Fix -Wsign-conversion: iterate over char and static_cast to unsigned char
    std::vector<uint32_t> tokens;
    tokens.reserve(piece.size());
    for (char c : piece) {
        tokens.push_back(static_cast<uint32_t>(static_cast<unsigned char>(c)));
    }

    // Standard BPE merge loop
    while (tokens.size() >= 2) {
        int best_rank = -1;
        size_t best_idx = 0;

        for (size_t i = 0; i < tokens.size() - 1; ++i) {
            uint64_t pair_key = make_pair_key(tokens[i], tokens[i + 1]);
            auto it = merge_ranks_.find(pair_key);
            if (it != merge_ranks_.end()) {
                if (best_rank == -1 || it->second < best_rank) {
                    best_rank = it->second;
                    best_idx = i;
                }
            }
        }

        if (best_rank == -1) {
            break;
        }

        tokens[best_idx] = static_cast<uint32_t>(256 + best_rank);
        tokens.erase(tokens.begin() + static_cast<std::ptrdiff_t>(best_idx + 1));
    }

    // Heuristic approximation for unmerged words:
    // Standard subword tokenizers average ~4 characters per token
    if (tokens.size() > 4) {
        size_t est = (piece.size() + 3) / 4;
        return std::min(tokens.size(), std::max(size_t{1}, est));
    }

    return tokens.size();
}

size_t BPETokenizer::count_tokens(std::string_view text) const {
    auto chunks = split_into_chunks(text);
    size_t total_tokens = 0;

    for (const auto& chunk : chunks) {
        total_tokens += count_word_tokens(chunk);
    }

    return total_tokens;
}

} // namespace snip
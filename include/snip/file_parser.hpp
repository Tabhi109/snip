#pragma once

#include "snip/parser.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace snip {

class FileParser : public DomainParser {
public:
    ParseResult parse(
        const std::vector<std::string>& cmd_args,
        std::string_view stdout_content,
        int exit_code
    ) override;

    // Skeletons out implementation bodies { ... } leaving signatures/declarations
    static std::string skeletonize_code(std::string_view code, std::string_view ext);

    // Formats directory listings/find outputs, skipping blacklisted vendor folders
    static std::string prune_tree(std::string_view content);
};

} // namespace snip
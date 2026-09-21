#pragma once

#include <string>
#include <string_view>

namespace snip {

class RecoveryCache {
public:
    // Writes raw output to /tmp/snip_last_run.log
    // Returns true on success
    static bool save_raw(std::string_view raw_content);

    // Path to the cached log
    static constexpr std::string_view CACHE_PATH = "/tmp/snip_last_run.log";
};

} // namespace snip
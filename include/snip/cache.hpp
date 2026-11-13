#pragma once

#include <string>
#include <string_view>

namespace snip {

class RecoveryCache {
public:
    // Writes raw output to last run recovery log
    // Returns true on success
    static bool save_raw(std::string_view raw_content);

    // Resolves writable cache path, falling back to user-specific log if /tmp/snip_last_run.log is un-writable
    static std::string get_cache_path();

    // Default static path to the cached log
    static constexpr std::string_view CACHE_PATH = "/tmp/snip_last_run.log";
};

} // namespace snip
#include "snip/cache.hpp"
#include <fstream>
#include <cstdlib>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace snip {

std::string RecoveryCache::get_cache_path() {
    std::string default_path = std::string(CACHE_PATH);
    std::ofstream test_out(default_path, std::ios::app);
    if (test_out.is_open()) {
        return default_path;
    }

#if !defined(_WIN32)
    std::string user_path = "/tmp/snip_" + std::to_string(::getuid()) + "_last_run.log";
    std::ofstream user_out(user_path, std::ios::app);
    if (user_out.is_open()) {
        return user_path;
    }
#endif

    const char* home = std::getenv("HOME");
    if (home) {
        return std::string(home) + "/.snip/last_run.log";
    }

    return default_path;
}

bool RecoveryCache::save_raw(std::string_view raw_content) {
    std::ofstream file(get_cache_path(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file.write(raw_content.data(), static_cast<std::streamsize>(raw_content.size()));
    return true;
}

} // namespace snip

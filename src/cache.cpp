#include "snip/cache.hpp"
#include <fstream>

namespace snip {

bool RecoveryCache::save_raw(std::string_view raw_content) {
    std::ofstream file(std::string(CACHE_PATH), std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file.write(raw_content.data(), static_cast<std::streamsize>(raw_content.size()));
    return true;
}

} // namespace snip
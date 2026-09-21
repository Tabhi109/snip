#include <catch2/catch_test_macros.hpp>
#include "snip/sanitizer.hpp"

TEST_CASE("Sanitizer strips ANSI escape color sequences", "[sanitizer]") {
    std::string raw = "\033[31m[ERROR]\033[0m Database connection failed\033[2K";
    std::string clean = snip::StreamSanitizer::sanitize(raw);
    REQUIRE(clean == "[ERROR] Database connection failed");
}

TEST_CASE("Sanitizer rewinds carriage return progress bars", "[sanitizer]") {
    std::string raw = "Downloading 10%\rDownloading 50%\rDownloading 100%\nDone.";
    std::string clean = snip::StreamSanitizer::sanitize(raw);
    REQUIRE(clean == "Downloading 100%\nDone.");
}

TEST_CASE("Sanitizer preserves standard CRLF newlines", "[sanitizer]") {
    std::string raw = "Line 1\r\nLine 2\r\n";
    std::string clean = snip::StreamSanitizer::sanitize(raw);
    REQUIRE(clean == "Line 1\nLine 2\n");
}
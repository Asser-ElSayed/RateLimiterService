#pragma once

#include <cctype>
#include <string>

namespace ratelimiter {

// Validates identifiers used to key rate limits (user ID, API key, or IP).
// Keeping this in one place ensures POST /check and GET /status/{key} apply
// exactly the same rule (AC5: invalid/missing key is rejected consistently).
class KeyValidator {
public:
    static constexpr size_t kMaxKeyLength = 128;

    // Allows letters, digits, and the punctuation commonly found in user
    // IDs, API keys, and IPv4/IPv6 addresses: '-', '_', '.', ':'.
    static bool isValid(const std::string& key) {
        if (key.empty() || key.size() > kMaxKeyLength) {
            return false;
        }
        for (unsigned char c : key) {
            const bool allowed = std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == ':';
            if (!allowed) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace ratelimiter

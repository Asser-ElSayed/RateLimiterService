#pragma once

#include <cstdint>
#include <string>

namespace ratelimiter {

// Supported rate-limiting algorithms for the MVP.
enum class Algorithm {
    FixedWindow,
    SlidingWindow,
    TokenBucket
};

// Converts an Algorithm value to its wire/string representation (used in JSON + logs).
inline std::string toString(Algorithm algorithm) {
    switch (algorithm) {
        case Algorithm::FixedWindow:   return "fixed_window";
        case Algorithm::SlidingWindow: return "sliding_window";
        case Algorithm::TokenBucket:   return "token_bucket";
    }
    return "unknown";
}

// Parses a wire/string representation into an Algorithm value.
// Returns false if the string does not match a known algorithm.
inline bool tryParseAlgorithm(const std::string& text, Algorithm& outAlgorithm) {
    if (text == "fixed_window") { outAlgorithm = Algorithm::FixedWindow; return true; }
    if (text == "sliding_window") { outAlgorithm = Algorithm::SlidingWindow; return true; }
    if (text == "token_bucket") { outAlgorithm = Algorithm::TokenBucket; return true; }
    return false;
}

// Active rate-limit configuration: how many requests are allowed per window,
// how long the window is, and which algorithm is applied.
// This is intentionally global/shared for the MVP (POST /config updates it for all keys).
struct RateLimitConfig {
    int limit = 5;                 // max allowed requests per window
    int64_t windowMs = 10000;      // window duration in milliseconds
    Algorithm algorithm = Algorithm::FixedWindow;
};

// Result of evaluating a rate-limit decision for a key.
// Used by both POST /check (mutating) and GET /status/{key} (read-only).
struct RateLimitDecision {
    bool allowed = false;
    int limit = 0;
    int remaining = 0;
    int64_t resetSeconds = 0;   // seconds until the window resets / next token is available
};

}  // namespace ratelimiter

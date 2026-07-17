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

// A single lookup table drives both directions of the string <-> enum
// conversion below, so the two functions can never fall out of sync with
// each other (there is exactly one place that lists the valid names).
struct AlgorithmName {
    Algorithm value;
    const char* text;
};

inline const AlgorithmName kAlgorithmNames[] = {
    {Algorithm::FixedWindow,   "fixed_window"},
    {Algorithm::SlidingWindow, "sliding_window"},
    {Algorithm::TokenBucket,   "token_bucket"},
};

// Converts an Algorithm value to its wire/string representation (used in JSON + logs).
inline std::string toString(Algorithm algorithm) {
    for (const auto& entry : kAlgorithmNames) {
        if (entry.value == algorithm) {
            return entry.text;
        }
    }
    return "unknown";
}

// Parses a wire/string representation into an Algorithm value.
// Returns false if the string does not match a known algorithm.
inline bool tryParseAlgorithm(const std::string& text, Algorithm& outAlgorithm) {
    for (const auto& entry : kAlgorithmNames) {
        if (text == entry.text) {
            outAlgorithm = entry.value;
            return true;
        }
    }
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

#pragma once

#include <mutex>
#include <string>

#include "RateLimitTypes.h"

namespace ratelimiter {

// Owns the currently active RateLimitConfig (limit, window, algorithm) and
// makes it safe to read from many worker threads while POST /config
// occasionally updates it.
//
// A single plain mutex guards every access. A shared/exclusive (reader-
// writer) lock would allow concurrent reads, but reads here are just a
// struct copy - cheap enough that a single mutex is simpler to reason
// about and fast enough for this MVP's traffic.
class ConfigManager {
public:
    explicit ConfigManager(RateLimitConfig initialConfig);

    RateLimitConfig get() const;

    // Validates and applies a new configuration. On success returns true and
    // leaves `outError` untouched. On failure returns false and sets
    // `outError` to a human-readable reason (used to build a 400 response).
    bool update(int limit, int64_t windowMs, Algorithm algorithm, std::string& outError);

private:
    mutable std::mutex mutex_;
    RateLimitConfig config_;
};

}  // namespace ratelimiter

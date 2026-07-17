#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "ConfigManager.h"
#include "IRateLimitStrategy.h"
#include "RateLimitTypes.h"

namespace ratelimiter {

// Central rate-limiting engine (the "Core" in the architecture diagram).
// Owns one instance of every supported strategy and, for each request,
// dispatches to whichever one is currently active in ConfigManager. The API
// layer only ever talks to RateLimiterCore - it has no knowledge of
// individual algorithms, which is the point of the Strategy pattern here.
class RateLimiterCore {
public:
    explicit RateLimiterCore(ConfigManager& configManager);

    // Evaluates and consumes one unit of quota for `key` under the current
    // configuration. Used by POST /check.
    RateLimitDecision checkAndConsume(const std::string& key);

    // Read-only status for `key` under the current configuration. Used by
    // GET /status/{key}.
    RateLimitDecision getStatus(const std::string& key) const;

    ConfigManager& config() { return configManager_; }

private:
    IRateLimitStrategy& strategyFor(Algorithm algorithm);
    const IRateLimitStrategy& strategyFor(Algorithm algorithm) const;

    ConfigManager& configManager_;
    std::unordered_map<Algorithm, std::unique_ptr<IRateLimitStrategy>> strategies_;
};

}  // namespace ratelimiter

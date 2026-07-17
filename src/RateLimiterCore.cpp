#include "RateLimiterCore.h"

#include "FixedWindowStrategy.h"
#include "SlidingWindowStrategy.h"
#include "TokenBucketStrategy.h"

namespace ratelimiter {

RateLimiterCore::RateLimiterCore(ConfigManager& configManager) : configManager_(configManager) {
    // Construct every supported strategy up front. Each owns its own
    // storage, so switching algorithms via POST /config does not lose or
    // mix state between algorithms - it simply changes which strategy is
    // consulted going forward.
    strategies_.emplace(Algorithm::FixedWindow, std::make_unique<FixedWindowStrategy>());
    strategies_.emplace(Algorithm::SlidingWindow, std::make_unique<SlidingWindowStrategy>());
    strategies_.emplace(Algorithm::TokenBucket, std::make_unique<TokenBucketStrategy>());
}

IRateLimitStrategy& RateLimiterCore::strategyFor(Algorithm algorithm) {
    return *strategies_.at(algorithm);
}

const IRateLimitStrategy& RateLimiterCore::strategyFor(Algorithm algorithm) const {
    return *strategies_.at(algorithm);
}

RateLimitDecision RateLimiterCore::checkAndConsume(const std::string& key) {
    const RateLimitConfig config = configManager_.get();
    return strategyFor(config.algorithm).checkAndConsume(key, config);
}

RateLimitDecision RateLimiterCore::getStatus(const std::string& key) const {
    const RateLimitConfig config = configManager_.get();
    return strategyFor(config.algorithm).getStatus(key, config);
}

}  // namespace ratelimiter

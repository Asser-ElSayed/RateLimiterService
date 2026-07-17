#include "ConfigManager.h"

namespace ratelimiter {

ConfigManager::ConfigManager(RateLimitConfig initialConfig) : config_(initialConfig) {}

RateLimitConfig ConfigManager::get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool ConfigManager::update(int limit, int64_t windowMs, Algorithm algorithm, std::string& outError) {
    if (limit <= 0) {
        outError = "limit must be a positive integer";
        return false;
    }
    if (windowMs <= 0) {
        outError = "window_ms must be a positive integer";
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    config_.limit = limit;
    config_.windowMs = windowMs;
    config_.algorithm = algorithm;
    return true;
}

}  // namespace ratelimiter

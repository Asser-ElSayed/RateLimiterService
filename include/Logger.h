#pragma once

#include <mutex>
#include <string>

namespace ratelimiter {

// Simple thread-safe logger writing structured, readable lines to stdout.
// Levels: INFO (allowed requests, config changes), WARN (blocked/rejected
// requests), ERROR (internal/unexpected failures).
//
// This is a singleton so every layer (API, Core, Storage) can log through
// the same instance without passing a reference everywhere.
class Logger {
public:
    static Logger& instance();

    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;

    void log(const char* level, const std::string& message);

    std::mutex mutex_;
};

}  // namespace ratelimiter

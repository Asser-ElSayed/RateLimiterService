#include <cstdlib>
#include <iostream>
#include <string>

#include "ApiServer.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "RateLimiterCore.h"

namespace {

// Reads an environment variable as an int, falling back to `fallback` if
// unset or not a valid integer. Lets the demo defaults be overridden
// without recompiling (e.g. PORT=9090).
int envOrDefault(const char* name, int fallback) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return fallback;
    }
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

}  // namespace

int main() {
    using namespace ratelimiter;

    // Default demo configuration: 5 requests per 10-second window, Fixed
    // Window algorithm. Override at runtime via POST /config.
    RateLimitConfig initialConfig;
    initialConfig.limit = envOrDefault("RATE_LIMIT", 5);
    initialConfig.windowMs = envOrDefault("RATE_WINDOW_MS", 10000);
    initialConfig.algorithm = Algorithm::FixedWindow;

    ConfigManager configManager(initialConfig);
    RateLimiterCore core(configManager);
    ApiServer server(core);

    const std::string host = "0.0.0.0";
    const int port = envOrDefault("PORT", 8080);

    Logger::instance().info("Starting C++ Rate Limiter Service");
    Logger::instance().info("Default config: limit=" + std::to_string(initialConfig.limit) +
                             " window_ms=" + std::to_string(initialConfig.windowMs) +
                             " algorithm=" + toString(initialConfig.algorithm));

    if (!server.run(host, port)) {
        Logger::instance().error("Failed to start server on " + host + ":" + std::to_string(port) +
                                  " (port may already be in use)");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

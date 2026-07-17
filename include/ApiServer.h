#pragma once

#include <string>

#include "httplib.h"
#include "RateLimiterCore.h"

namespace ratelimiter {

// HTTP API layer (Controllers). Owns the httplib server, translates HTTP
// requests into RateLimiterCore calls, and formats JSON responses. This is
// the only layer that knows about HTTP - RateLimiterCore, the strategies,
// and the storage layer are all completely HTTP-agnostic.
class ApiServer {
public:
    explicit ApiServer(RateLimiterCore& core);

    // Starts listening and blocks the calling thread (httplib serves
    // requests using its own internal worker thread pool, so concurrent
    // requests are handled without any extra setup here).
    bool run(const std::string& host, int port);

private:
    void registerRoutes();

    void handleCheck(const httplib::Request& req, httplib::Response& res);
    void handleStatus(const httplib::Request& req, httplib::Response& res);
    void handleConfig(const httplib::Request& req, httplib::Response& res);
    void handleHealth(const httplib::Request& req, httplib::Response& res);

    RateLimiterCore& core_;
    httplib::Server server_;
};

}  // namespace ratelimiter

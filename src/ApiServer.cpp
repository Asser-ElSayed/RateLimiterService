#include "ApiServer.h"

#include "KeyValidator.h"
#include "Logger.h"
#include "json.hpp"

namespace ratelimiter {

using json = nlohmann::json;

namespace {

constexpr const char* kJsonContentType = "application/json";

void writeError(httplib::Response& res, int status, const std::string& message) {
    json body;
    body["error"] = message;
    res.status = status;
    res.set_content(body.dump(), kJsonContentType);
}

json decisionToJson(const RateLimitDecision& decision) {
    json body;
    body["allowed"] = decision.allowed;
    body["limit"] = decision.limit;
    body["remaining"] = decision.remaining;
    body["reset_seconds"] = decision.resetSeconds;
    return body;
}

// Extracts "key" from a JSON request body. Returns false (and leaves
// outKey untouched) if the body is not a JSON object or "key" is missing
// or not a string - the caller treats that as an invalid request.
bool tryExtractKey(const json& body, std::string& outKey) {
    if (!body.is_object() || !body.contains("key") || !body["key"].is_string()) {
        return false;
    }
    outKey = body["key"].get<std::string>();
    return true;
}

}  // namespace

ApiServer::ApiServer(RateLimiterCore& core) : core_(core) {
    registerRoutes();
}

void ApiServer::registerRoutes() {
    server_.Post("/check", [this](const httplib::Request& req, httplib::Response& res) {
        handleCheck(req, res);
    });

    // Captures everything after /status/ as the key (group 1).
    server_.Get(R"(/status/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
        handleStatus(req, res);
    });

    server_.Post("/config", [this](const httplib::Request& req, httplib::Response& res) {
        handleConfig(req, res);
    });

    server_.Get("/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealth(req, res);
    });

    // Catches anything thrown by a handler above so a single bad request
    // can never crash the process or leak internal details to the client.
    server_.set_exception_handler([](const httplib::Request&, httplib::Response& res,
                                      const std::exception_ptr& ep) {
        std::string detail;
        try {
            if (ep) std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            detail = e.what();
        } catch (...) {
            detail = "unknown error";
        }
        Logger::instance().error("Unhandled exception while processing request: " + detail);
        writeError(res, 500, "internal server error");
    });

    server_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.body.empty()) {
            writeError(res, res.status, "not found");
        }
    });
}

void ApiServer::handleCheck(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = req.body.empty() ? json::object() : json::parse(req.body);
    } catch (const json::parse_error&) {
        Logger::instance().warn("POST /check rejected: malformed JSON body");
        writeError(res, 400, "request body must be valid JSON");
        return;
    }

    std::string key;
    if (!tryExtractKey(body, key) || !KeyValidator::isValid(key)) {
        Logger::instance().warn("POST /check rejected: missing or invalid 'key'");
        writeError(res, 400, "field 'key' is required and must be a valid identifier "
                              "(1-128 chars; letters, digits, '-', '_', '.', ':')");
        return;
    }

    const RateLimitDecision decision = core_.checkAndConsume(key);

    if (decision.allowed) {
        Logger::instance().info("POST /check key=" + key + " -> allowed, remaining=" +
                                 std::to_string(decision.remaining));
        res.status = 200;
    } else {
        Logger::instance().warn("POST /check key=" + key + " -> blocked (limit reached)");
        res.status = 429;
        res.set_header("Retry-After", std::to_string(decision.resetSeconds));
    }
    res.set_content(decisionToJson(decision).dump(), kJsonContentType);
}

void ApiServer::handleStatus(const httplib::Request& req, httplib::Response& res) {
    if (req.matches.size() < 2) {
        writeError(res, 400, "key is required in the path: /status/{key}");
        return;
    }
    const std::string key = req.matches[1];

    if (!KeyValidator::isValid(key)) {
        Logger::instance().warn("GET /status rejected: invalid key");
        writeError(res, 400, "invalid key format");
        return;
    }

    const RateLimitDecision decision = core_.getStatus(key);
    Logger::instance().info("GET /status key=" + key + " -> remaining=" +
                             std::to_string(decision.remaining));
    res.status = 200;
    res.set_content(decisionToJson(decision).dump(), kJsonContentType);
}

void ApiServer::handleConfig(const httplib::Request& req, httplib::Response& res) {
    json body;
    try {
        body = req.body.empty() ? json::object() : json::parse(req.body);
    } catch (const json::parse_error&) {
        Logger::instance().warn("POST /config rejected: malformed JSON body");
        writeError(res, 400, "request body must be valid JSON");
        return;
    }

    if (!body.is_object() || !body.contains("limit") || !body.contains("window_ms") ||
        !body.contains("algorithm") || !body["limit"].is_number_integer() ||
        !body["window_ms"].is_number_integer() || !body["algorithm"].is_string()) {
        writeError(res, 400,
                   "expected { \"limit\": <int>, \"window_ms\": <int>, "
                   "\"algorithm\": \"fixed_window\"|\"sliding_window\"|\"token_bucket\" }");
        return;
    }

    Algorithm algorithm;
    if (!tryParseAlgorithm(body["algorithm"].get<std::string>(), algorithm)) {
        writeError(res, 400,
                   "unknown algorithm; expected fixed_window, sliding_window, or token_bucket");
        return;
    }

    const int limit = body["limit"].get<int>();
    const int64_t windowMs = body["window_ms"].get<int64_t>();

    std::string error;
    if (!core_.config().update(limit, windowMs, algorithm, error)) {
        Logger::instance().warn("POST /config rejected: " + error);
        writeError(res, 400, error);
        return;
    }

    Logger::instance().info("POST /config updated: limit=" + std::to_string(limit) +
                             " window_ms=" + std::to_string(windowMs) +
                             " algorithm=" + toString(algorithm));

    json responseBody;
    responseBody["limit"] = limit;
    responseBody["window_ms"] = windowMs;
    responseBody["algorithm"] = toString(algorithm);
    res.status = 200;
    res.set_content(responseBody.dump(), kJsonContentType);
}

void ApiServer::handleHealth(const httplib::Request&, httplib::Response& res) {
    json body;
    body["status"] = "ok";
    res.status = 200;
    res.set_content(body.dump(), kJsonContentType);
}

bool ApiServer::run(const std::string& host, int port) {
    Logger::instance().info("Rate limiter service listening on " + host + ":" + std::to_string(port));
    return server_.listen(host, port);
}

}  // namespace ratelimiter

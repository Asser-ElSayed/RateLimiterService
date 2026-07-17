#pragma once

#include <chrono>
#include <cstdint>

namespace ratelimiter {

// Returns the current time as milliseconds since the Unix epoch.
// Centralized so every strategy measures time the same way (and so tests
// could inject a fake clock later if needed).
inline int64_t nowMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace ratelimiter

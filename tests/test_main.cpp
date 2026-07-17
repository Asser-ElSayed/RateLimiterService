// Lightweight, dependency-free test suite. Deliberately avoids pulling in a
// test framework (Catch2/GoogleTest) to keep the project trivial to build in
// Visual Studio - this compiles as a second, small executable that talks
// directly to RateLimiterCore (no HTTP layer involved) and exercises the
// acceptance criteria from the project brief.
//
// Run it (from the build output folder): RateLimiterTests.exe
// A non-zero exit code means at least one check failed.

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "ConfigManager.h"
#include "KeyValidator.h"
#include "RateLimiterCore.h"

using namespace ratelimiter;

namespace {

int g_checksRun = 0;
int g_checksFailed = 0;

void check(bool condition, const std::string& description) {
    ++g_checksRun;
    if (condition) {
        std::cout << "  [PASS] " << description << "\n";
    } else {
        ++g_checksFailed;
        std::cout << "  [FAIL] " << description << "\n";
    }
}

// AC1 + AC2 + AC3: requests within limit are allowed, requests beyond the
// limit are blocked, and every decision carries limit/remaining/reset info.
void test_fixedWindow_allowsThenBlocks() {
    std::cout << "test_fixedWindow_allowsThenBlocks (AC1, AC2, AC3)\n";

    ConfigManager configManager(RateLimitConfig{3, 60000, Algorithm::FixedWindow});
    RateLimiterCore core(configManager);

    for (int i = 1; i <= 3; ++i) {
        const auto decision = core.checkAndConsume("user-a");
        check(decision.allowed, "request " + std::to_string(i) + " within limit is allowed");
        check(decision.limit == 3, "decision reports configured limit");
        check(decision.remaining == 3 - i, "remaining decreases as quota is consumed");
    }

    const auto blocked = core.checkAndConsume("user-a");
    check(!blocked.allowed, "4th request exceeding limit is blocked");
    check(blocked.remaining == 0, "blocked response reports zero remaining");
    check(blocked.resetSeconds >= 0, "blocked response reports a reset/retry time");
}

// AC6: once the window elapses, requests are allowed again.
void test_fixedWindow_resetsAfterWindow() {
    std::cout << "test_fixedWindow_resetsAfterWindow (AC6)\n";

    ConfigManager configManager(RateLimitConfig{2, 200, Algorithm::FixedWindow});
    RateLimiterCore core(configManager);

    check(core.checkAndConsume("user-b").allowed, "1st request allowed");
    check(core.checkAndConsume("user-b").allowed, "2nd request allowed");
    check(!core.checkAndConsume("user-b").allowed, "3rd request blocked (limit reached)");

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    check(core.checkAndConsume("user-b").allowed, "request after window reset is allowed again");
}

// AC6 for Token Bucket specifically: tokens refill over time.
void test_tokenBucket_refillsOverTime() {
    std::cout << "test_tokenBucket_refillsOverTime (AC6)\n";

    ConfigManager configManager(RateLimitConfig{2, 200, Algorithm::TokenBucket});
    RateLimiterCore core(configManager);

    check(core.checkAndConsume("user-c").allowed, "1st request consumes a token");
    check(core.checkAndConsume("user-c").allowed, "2nd request consumes last token");
    check(!core.checkAndConsume("user-c").allowed, "3rd request blocked (bucket empty)");

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    check(core.checkAndConsume("user-c").allowed, "request after refill window is allowed again");
}

// Sanity check for the Sliding Window strategy: stays within its limit.
void test_slidingWindow_blocksOverLimit() {
    std::cout << "test_slidingWindow_blocksOverLimit (AC1, AC2)\n";

    ConfigManager configManager(RateLimitConfig{3, 60000, Algorithm::SlidingWindow});
    RateLimiterCore core(configManager);

    int allowedCount = 0;
    for (int i = 0; i < 5; ++i) {
        if (core.checkAndConsume("user-d").allowed) ++allowedCount;
    }
    check(allowedCount == 3, "sliding window allows exactly the configured limit");
}

// AC3: GET /status equivalent (Core::getStatus) must not consume quota.
void test_status_isReadOnly() {
    std::cout << "test_status_isReadOnly (AC3)\n";

    ConfigManager configManager(RateLimitConfig{2, 60000, Algorithm::FixedWindow});
    RateLimiterCore core(configManager);

    const auto before = core.getStatus("user-e");
    check(before.remaining == 2, "status before any request reports full quota");

    core.checkAndConsume("user-e");
    const auto afterOneCheck = core.getStatus("user-e");
    check(afterOneCheck.remaining == 1, "status reflects quota consumed by /check");

    const auto statusAgain = core.getStatus("user-e");
    check(statusAgain.remaining == 1, "calling status again does not consume further quota");
}

// AC4: concurrent requests for the same key never allow more than the
// configured limit, and no counter updates are lost/corrupted.
void test_concurrency_doesNotExceedLimit() {
    std::cout << "test_concurrency_doesNotExceedLimit (AC4)\n";

    constexpr int kLimit = 100;
    constexpr int kThreads = 8;
    constexpr int kAttemptsPerThread = 50;  // 400 total attempts against a limit of 100

    ConfigManager configManager(RateLimitConfig{kLimit, 60000, Algorithm::FixedWindow});
    RateLimiterCore core(configManager);

    std::atomic<int> allowedCount{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&core, &allowedCount]() {
            for (int i = 0; i < kAttemptsPerThread; ++i) {
                if (core.checkAndConsume("shared-key").allowed) {
                    allowedCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& t : threads) t.join();

    check(allowedCount.load() == kLimit,
          "exactly " + std::to_string(kLimit) + " of " +
              std::to_string(kThreads * kAttemptsPerThread) +
              " concurrent requests were allowed (got " + std::to_string(allowedCount.load()) + ")");
}

// AC5: invalid/missing keys are rejected.
void test_keyValidator_rejectsInvalidKeys() {
    std::cout << "test_keyValidator_rejectsInvalidKeys (AC5)\n";

    check(!KeyValidator::isValid(""), "empty key is rejected");
    check(!KeyValidator::isValid(std::string(200, 'a')), "overly long key is rejected");
    check(!KeyValidator::isValid("bad key with spaces"), "key with spaces is rejected");
    check(!KeyValidator::isValid("bad/key"), "key with slash is rejected");

    check(KeyValidator::isValid("user-123"), "alphanumeric + dash key is accepted");
    check(KeyValidator::isValid("192.168.0.1"), "IPv4-style key is accepted");
    check(KeyValidator::isValid("api_key:tenant42"), "underscore/colon key is accepted");
}

// AC5 (config validation half): invalid config values are rejected.
void test_configManager_rejectsInvalidValues() {
    std::cout << "test_configManager_rejectsInvalidValues (AC5)\n";

    ConfigManager configManager(RateLimitConfig{5, 10000, Algorithm::FixedWindow});
    std::string error;

    check(!configManager.update(0, 1000, Algorithm::FixedWindow, error), "zero limit is rejected");
    check(!configManager.update(5, 0, Algorithm::FixedWindow, error), "zero window is rejected");
    check(configManager.update(10, 5000, Algorithm::TokenBucket, error), "valid update is accepted");
    check(configManager.get().limit == 10, "accepted update is reflected in current config");
}

}  // namespace

int main() {
    std::cout << "Running C++ Rate Limiter Service test suite\n";
    std::cout << "=============================================\n";

    test_fixedWindow_allowsThenBlocks();
    test_fixedWindow_resetsAfterWindow();
    test_tokenBucket_refillsOverTime();
    test_slidingWindow_blocksOverLimit();
    test_status_isReadOnly();
    test_concurrency_doesNotExceedLimit();
    test_keyValidator_rejectsInvalidKeys();
    test_configManager_rejectsInvalidValues();

    std::cout << "=============================================\n";
    std::cout << g_checksRun - g_checksFailed << "/" << g_checksRun << " checks passed\n";

    return g_checksFailed == 0 ? 0 : 1;
}

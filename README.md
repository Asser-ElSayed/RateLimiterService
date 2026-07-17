# C++ Rate Limiter Service

A backend service in C++17 that limits how many requests a client (identified
by user ID, API key, or IP) can make within a configured time window. Exposes
a small HTTP/JSON API and supports three pluggable algorithms — **Fixed
Window**, **Sliding Window**, and **Token Bucket** — behind a common
`IRateLimitStrategy` interface (Strategy design pattern). State is tracked
in memory and is thread-safe under concurrent load.

Out of scope for this MVP (by design, per the project brief): distributed
rate limiting, Redis, dashboards, authentication, persistent storage.

---

## 1. Project layout

```
RateLimiterService/
├── CMakeLists.txt            Build configuration (see "Build & Run" below)
├── include/                  Public headers (interfaces, types, classes)
│   ├── RateLimitTypes.h      Algorithm enum, RateLimitConfig, RateLimitDecision
│   ├── IRateLimitStrategy.h  Strategy interface implemented by every algorithm
│   ├── FixedWindowStrategy.h
│   ├── SlidingWindowStrategy.h
│   ├── TokenBucketStrategy.h
│   ├── ShardedKeyStore.h     Thread-safe storage/state layer (generic, templated)
│   ├── RateLimiterCore.h     Dispatches requests to the active strategy
│   ├── ConfigManager.h       Thread-safe holder of the active configuration
│   ├── KeyValidator.h        Validates request keys (AC5)
│   ├── Logger.h               Structured, thread-safe logging
│   ├── TimeUtil.h            Shared clock helper
│   └── ApiServer.h           HTTP layer (controllers) — the only HTTP-aware header
├── src/                      Implementations (one .cpp per header above, plus main.cpp)
├── tests/
│   └── test_main.cpp         Dependency-free test suite exercising AC1–AC6
└── third_party/              Header-only dependencies (vendored, no package manager needed)
    ├── httplib.h              cpp-httplib v0.15.3 (HTTP server)
    └── json.hpp                nlohmann/json v3.11.3 (JSON parsing)
```

File and class names map 1:1 to the responsibilities in the project brief
(one file per strategy, one file per architectural layer) so it's easy to
find where a given requirement is implemented.

---

## 2. Architecture

```
Client → API Layer (ApiServer)
            │  POST /check, GET /status/{key}, POST /config
            ▼
       RateLimiterCore  ── reads current settings from ──▶  ConfigManager
            │  (Strategy pattern dispatch)
            ▼
   IRateLimitStrategy  ─┬─ FixedWindowStrategy
                         ├─ SlidingWindowStrategy
                         └─ TokenBucketStrategy
            │  (each owns its own storage)
            ▼
     ShardedKeyStore<T>   (thread-safe, sharded-mutex in-memory storage)
```

* **API Layer** (`ApiServer`) is the only class that knows about HTTP. It
  validates requests, calls `RateLimiterCore`, and formats JSON responses.
* **RateLimiterCore** doesn't know how any algorithm works — it just asks
  `ConfigManager` which algorithm is active and forwards the call to the
  matching `IRateLimitStrategy`. Adding a 4th algorithm later means writing
  one new class and registering it in `RateLimiterCore`'s constructor —
  nothing else changes.
* **Storage layer** (`ShardedKeyStore<T>`) is a small generic template, not
  tied to any one algorithm. It shards keys across N independent mutexes so
  concurrent requests for *different* keys never block each other.

---

## 3. Building & running in Visual Studio

Requires **Visual Studio 2019 or later** with the **"Desktop development
with C++"** workload installed (this provides the MSVC compiler and CMake
Tools for C++ — no other install is needed; both dependencies are already
vendored in `third_party/`).

### Option A — Open Folder (recommended, no project files to maintain)

1. `File → Open → Folder…` and select the `RateLimiterService` folder.
2. Visual Studio detects `CMakeLists.txt` automatically and configures the
   project (watch the *Output* window; first run takes a few seconds).
3. In the toolbar dropdown, pick **RateLimiterService.exe** as the startup
   item, then `Build → Build All` (or `Ctrl+Shift+B`).
4. Press `Ctrl+F5` (Start Without Debugging) to run it. You should see:
   ```
   [INFO] Starting C++ Rate Limiter Service
   [INFO] Default config: limit=5 window_ms=10000 algorithm=fixed_window
   [INFO] Rate limiter service listening on 0.0.0.0:8080
   ```
5. To run the test suite instead, switch the startup item dropdown to
   **RateLimiterTests.exe** and run it the same way.

### Option B — Generate a .sln with CMake

```bat
cmake -G "Visual Studio 17 2022" -A x64 -S . -B build
```

Then open `build\RateLimiterService.sln` in Visual Studio and build/run
normally (`RateLimiterService` and `RateLimiterTests` both appear as
projects in Solution Explorer).

### Building from the command line (any platform)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/RateLimiterService        # (build/Release/RateLimiterService.exe on Windows)
./build/RateLimiterTests          # (build/Release/RateLimiterTests.exe on Windows)
```

### Configuration at startup

The server reads three optional environment variables; sensible defaults
are used if they're not set:

| Variable         | Default | Meaning                          |
|-------------------|---------|-----------------------------------|
| `PORT`            | `8080`  | TCP port to listen on             |
| `RATE_LIMIT`      | `5`     | Default requests allowed per window |
| `RATE_WINDOW_MS`  | `10000` | Default window length, milliseconds |

The default algorithm at startup is Fixed Window; use `POST /config` to
change limit, window, and/or algorithm at runtime (see below).

---

## 4. API reference

All request/response bodies are JSON (`Content-Type: application/json`).

### `POST /check` — evaluate and consume quota for a key

Request:
```json
{ "key": "user-123" }
```

Response `200 OK` (allowed):
```json
{ "allowed": true, "limit": 5, "remaining": 4, "reset_seconds": 8 }
```

Response `429 Too Many Requests` (blocked), also sets a `Retry-After` header:
```json
{ "allowed": false, "limit": 5, "remaining": 0, "reset_seconds": 3 }
```

Response `400 Bad Request` (missing/invalid key or malformed JSON):
```json
{ "error": "field 'key' is required and must be a valid identifier ..." }
```

### `GET /status/{key}` — read current limiter state without consuming quota

```
GET /status/user-123
→ 200 OK
{ "allowed": true, "limit": 5, "remaining": 3, "reset_seconds": 6 }
```

`400 Bad Request` if the key fails validation.

### `POST /config` — update rate-limit settings (testing/demo use)

Request:
```json
{ "limit": 10, "window_ms": 60000, "algorithm": "token_bucket" }
```
`algorithm` must be one of `"fixed_window"`, `"sliding_window"`, `"token_bucket"`.

Response `200 OK` echoes the applied config; `400 Bad Request` if `limit`/
`window_ms` aren't positive integers or `algorithm` is unrecognized.

### `GET /health` — liveness check (bonus, not in the original brief)

```json
{ "status": "ok" }
```

### Example session (curl)

```bash
curl -X POST http://localhost:8080/check -H "Content-Type: application/json" -d "{\"key\":\"user-123\"}"
curl http://localhost:8080/status/user-123
curl -X POST http://localhost:8080/config -H "Content-Type: application/json" -d "{\"limit\":10,\"window_ms\":60000,\"algorithm\":\"sliding_window\"}"
```

---

## 5. Concurrency & error handling

* **Thread safety (AC4):** the storage layer shards keys across 16
  independent mutexes; two requests for different keys almost never
  contend, and requests for the *same* key are serialized just long enough
  to perform an atomic read-modify-write of that key's counters. `httplib`
  serves requests using its own worker thread pool, so real concurrent
  HTTP traffic exercises this path directly (verified — see §6).
* **Token Bucket refill** is computed lazily from elapsed time on each
  request rather than via a background timer thread, so there's no extra
  thread per key and no timing drift between a timer and actual requests.
* **Error handling (AC5):** the API layer validates the key format and
  JSON shape *before* calling into the Core. Invalid input → `400` with a
  clear message; over limit → `429`; anything unexpected (should not
  normally happen) is caught by a global exception handler → `500` with a
  generic message to the client and the real detail written to the log.
* **Logging:** `Logger` is a thread-safe singleton; every allowed (`INFO`),
  blocked (`WARN`), or failed (`WARN`/`ERROR`) request is logged with a
  timestamp, e.g. `[2026-07-16 21:06:39.883] [WARN] POST /check key=... -> blocked (limit reached)`.

---

## 6. Testing

`RateLimiterTests` is a small, dependency-free executable (no test
framework required) that calls `RateLimiterCore` directly and checks each
acceptance criterion:

```
Running C++ Rate Limiter Service test suite
=============================================
test_fixedWindow_allowsThenBlocks (AC1, AC2, AC3)      ✓
test_fixedWindow_resetsAfterWindow (AC6)                ✓
test_tokenBucket_refillsOverTime (AC6)                  ✓
test_slidingWindow_blocksOverLimit (AC1, AC2)           ✓
test_status_isReadOnly (AC3)                            ✓
test_concurrency_doesNotExceedLimit (AC4)               ✓
test_keyValidator_rejectsInvalidKeys (AC5)              ✓
test_configManager_rejectsInvalidValues (AC5)           ✓
=============================================
36/36 checks passed
```

This project was also built and run end-to-end against the real HTTP
server during development (concurrent `curl` load included: 200 parallel
requests against a limit of 50 returned exactly 50× `200` and 150× `429`,
with no over- or under-counting).

---

## 7. Acceptance criteria traceability

| # | Criterion | Where it's satisfied |
|---|-----------|------------------------|
| AC1 | Valid request within limit is allowed | `IRateLimitStrategy::checkAndConsume` implementations; `ApiServer::handleCheck` → `200` |
| AC2 | Request exceeding the limit is blocked with a clear response (429) | Same as above; `ApiServer::handleCheck` sets `res.status = 429` + `Retry-After` header |
| AC3 | Response includes limit, remaining, retry/reset metadata | `RateLimitDecision` struct; `decisionToJson()` in `ApiServer.cpp` |
| AC4 | Concurrent requests don't corrupt counters or over-allow | `ShardedKeyStore<T>` (per-shard mutex, atomic read-modify-write); `test_concurrency_doesNotExceedLimit` |
| AC5 | Invalid/missing key is rejected with a clear error | `KeyValidator::isValid`; `ApiServer::handleCheck` / `handleStatus` → `400` |
| AC6 | Requests allowed again after window reset/refill | Window-rollover logic in each strategy; `test_fixedWindow_resetsAfterWindow`, `test_tokenBucket_refillsOverTime` |
| AC7 | Build/run instructions provided; project compiles | This README (§3); verified via clean `cmake --build` with zero warnings/errors |

---

## 8. Notes & assumptions

* Configuration set via `POST /config` is global (applies to all keys), not
  per-key — matching the brief's "configurable limit values and durations"
  without introducing per-client config storage, which was out of scope.
* Switching `algorithm` via `POST /config` does not mix state between
  algorithms: each strategy owns its own store, so a key's history under
  Fixed Window is independent of its history under Token Bucket.
* State is in-memory only and is lost on restart — expected for this MVP
  (no persistence, no Redis, per the brief).

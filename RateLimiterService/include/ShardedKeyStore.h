#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace ratelimiter {

// Generic thread-safe storage for per-key state (Storage / State layer).
//
// Instead of one global mutex guarding the whole map (which would serialize
// every request regardless of key), keys are hashed into a fixed number of
// independent shards, each with its own mutex. Two requests for different
// keys can proceed fully in parallel; only requests for the *same* key
// (which land in the same shard) ever contend for a lock, and that
// contention is exactly what keeps counters correct (satisfies AC4).
//
// TState is the algorithm-specific per-key state (e.g. fixed-window
// counters, token-bucket tokens). Each strategy instantiates its own store
// with its own state type, keeping storage generic and reusable.
//
// The shard count is a fixed compile-time constant rather than a
// constructor parameter: no caller ever needed a different count, so a
// plain std::array of shards replaces the original std::vector + custom
// constructor, with one less moving part to reason about.
template <typename TState>
class ShardedKeyStore {
public:
    // Looks up the state for `key`, creating it via `makeDefault` if it
    // does not exist yet, then applies `mutate` to it while holding the
    // shard lock. Returns whatever `mutate` returns.
    template <typename DefaultFactory, typename Mutator>
    auto withState(const std::string& key, DefaultFactory&& makeDefault, Mutator&& mutate) {
        Shard& shard = shardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            it = shard.data.emplace(key, makeDefault()).first;
        }
        return mutate(it->second);
    }

    // Read-only peek at a key's state without creating an entry if absent.
    // Returns std::nullopt if the key has no recorded state yet.
    std::optional<TState> peek(const std::string& key) const {
        const Shard& shard = shardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    bool contains(const std::string& key) const {
        const Shard& shard = shardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        return shard.data.find(key) != shard.data.end();
    }

private:
    static constexpr size_t kShardCount = 16;

    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<std::string, TState> data;
    };

    Shard& shardFor(const std::string& key) {
        return shards_[std::hash<std::string>{}(key) % kShardCount];
    }
    const Shard& shardFor(const std::string& key) const {
        return shards_[std::hash<std::string>{}(key) % kShardCount];
    }

    // A fixed-size array needs no allocation and no constructor parameter;
    // std::mutex is non-movable, but that's not a problem here since the
    // array is never resized or copied after construction.
    std::array<Shard, kShardCount> shards_;
};

}  // namespace ratelimiter

#pragma once

#include <array>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
template <typename TState>
class ShardedKeyStore {
public:
    explicit ShardedKeyStore(size_t shardCount = 16)
        : shards_(shardCount) {}

    // Looks up the state for `key`, creating it via `defaultFactory` if it
    // does not exist yet, then applies `mutator` to it while holding the
    // shard lock. Returns whatever `mutator` returns.
    template <typename Mutator, typename DefaultFactory>
    auto withState(const std::string& key, DefaultFactory&& defaultFactory, Mutator&& mutator)
        -> decltype(mutator(std::declval<TState&>())) {
        Shard& shard = shardFor(key);
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            it = shard.data.emplace(key, defaultFactory()).first;
        }
        return mutator(it->second);
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
    struct Shard {
        mutable std::mutex mutex;
        std::unordered_map<std::string, TState> data;
    };

    Shard& shardFor(const std::string& key) {
        return shards_[std::hash<std::string>{}(key) % shards_.size()];
    }
    const Shard& shardFor(const std::string& key) const {
        return shards_[std::hash<std::string>{}(key) % shards_.size()];
    }

    // std::vector<Shard> would require Shard to be movable; std::mutex is not,
    // so shards are allocated once and never resized/moved after construction.
    std::vector<Shard> shards_;
};

}  // namespace ratelimiter

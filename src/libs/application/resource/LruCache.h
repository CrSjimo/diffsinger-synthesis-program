#ifndef DSSP_RESOURCE_LRUCACHE_H
#define DSSP_RESOURCE_LRUCACHE_H

#include <chrono>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <dsspresource/CacheOptions.h>

namespace dssp::resource {

    // Values own their cleanup. Leases pin entries; eviction never destroys a
    // leased value. Loading and exclusive access are the caller's responsibility.
    template <class Key, class Value, class Compare = std::less<Key>>
    class LruCache {
        using Clock = std::chrono::steady_clock;
        struct Entry;
        using EntryPtr = std::shared_ptr<Entry>;
        using Order = std::list<EntryPtr>;
        struct Entry {
            Key key;
            Value value;
            Clock::time_point lastUsed;
            std::size_t leases = 1;
            bool listed = true;
            typename Order::iterator position{};
        };
        struct State {
            explicit State(CacheOptions options) : options(options) {}
            CacheOptions options;
            std::mutex mutex;
            std::map<Key, EntryPtr, Compare> entries;
            Order order;

            void remove(const EntryPtr &entry, std::vector<EntryPtr> &retired) {
                entry->listed = false;
                entries.erase(entry->key);
                order.erase(entry->position);
                retired.push_back(entry);
            }
            void trim(std::vector<EntryPtr> &retired) {
                if (!options.capacity) {
                    return;
                }
                auto it = order.end();
                while (entries.size() > options.capacity && it != order.begin()) {
                    const auto entry = *--it;
                    if (!entry->leases) {
                        ++it;
                        remove(entry, retired);
                    }
                }
            }
            void release(EntryPtr entry) {
                std::vector<EntryPtr> retired;
                {
                    const std::lock_guard lock(mutex);
                    --entry->leases;
                    if (entry->listed && !entry->leases) {
                        entry->lastUsed = Clock::now();
                        order.splice(order.begin(), order, entry->position);
                        trim(retired);
                    }
                }
                // Destruction may perform I/O or wait for a resource. It must
                // happen after the cache mutex is released.
            }
        };

    public:
        class Lease {
        public:
            Lease(const Lease &) = delete;
            Lease &operator=(const Lease &) = delete;
            Lease(Lease &&other) noexcept : state_(std::move(other.state_)), entry_(std::move(other.entry_)) {}
            Lease &operator=(Lease &&other) noexcept {
                if (this != &other) {
                    reset();
                    state_ = std::move(other.state_);
                    entry_ = std::move(other.entry_);
                }
                return *this;
            }
            ~Lease() { reset(); }
            Value &value() const noexcept { return entry_->value; }

        private:
            friend class LruCache;
            Lease(std::shared_ptr<State> state, EntryPtr entry) : state_(std::move(state)), entry_(std::move(entry)) {}
            void reset() {
                if (entry_) {
                    state_->release(std::move(entry_));
                    state_.reset();
                }
            }
            std::shared_ptr<State> state_;
            EntryPtr entry_;
        };

        explicit LruCache(CacheOptions options = {}) : state_(std::make_shared<State>(options)) {}
        ~LruCache() { clear(); }
        LruCache(const LruCache &) = delete;
        LruCache &operator=(const LruCache &) = delete;

        std::optional<Lease> acquire(const Key &key) {
            const std::lock_guard lock(state_->mutex);
            const auto it = state_->entries.find(key);
            if (it == state_->entries.end()) {
                return {};
            }
            const auto &entry = it->second;
            ++entry->leases;
            entry->lastUsed = Clock::now();
            state_->order.splice(state_->order.begin(), state_->order, entry->position);
            return Lease(state_, entry);
        }
        Lease insert(Key key, Value value) {
            auto entry = std::make_shared<Entry>(Entry{std::move(key), std::move(value), Clock::now()});
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(state_->mutex);
                if (const auto old = state_->entries.find(entry->key); old != state_->entries.end()) {
                    const auto previous = old->second;
                    state_->remove(previous, retired);
                }
                state_->order.push_front(entry);
                entry->position = state_->order.begin();
                state_->entries.emplace(entry->key, entry);
                state_->trim(retired);
            }
            return Lease(state_, std::move(entry));
        }
        void erase(const Key &key) {
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(state_->mutex);
                if (const auto it = state_->entries.find(key); it != state_->entries.end()) {
                    const auto entry = it->second;
                    state_->remove(entry, retired);
                }
            }
        }
        void sweep(Clock::time_point now = Clock::now()) {
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(state_->mutex);
                if (state_->options.idleTimeout.count() > 0) {
                    for (auto it = state_->order.begin(); it != state_->order.end();) {
                        const auto entry = *it++;
                        if (!entry->leases && std::chrono::duration_cast<std::chrono::milliseconds>(now - entry->lastUsed) >= state_->options.idleTimeout) {
                            state_->remove(entry, retired);
                        }
                    }
                }
                state_->trim(retired);
            }
        }
        void clear() {
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(state_->mutex);
                while (!state_->order.empty()) {
                    const auto entry = state_->order.front();
                    state_->remove(entry, retired);
                }
            }
        }
        std::size_t size() const {
            const std::lock_guard lock(state_->mutex);
            return state_->entries.size();
        }

    private:
        std::shared_ptr<State> state_;
    };
}

#endif // DSSP_RESOURCE_LRUCACHE_H

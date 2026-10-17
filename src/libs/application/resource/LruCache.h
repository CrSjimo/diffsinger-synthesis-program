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
            Lease(Lease &&other) noexcept : m_state(std::move(other.m_state)), m_entry(std::move(other.m_entry)) {}

            Lease &operator=(Lease &&other) noexcept {
                if (this != &other) {
                    reset();
                    m_state = std::move(other.m_state);
                    m_entry = std::move(other.m_entry);
                }
                return *this;
            }

            ~Lease() { reset(); }
            Value &value() const noexcept { return m_entry->value; }

        private:
            friend class LruCache;
            Lease(std::shared_ptr<State> state, EntryPtr entry) : m_state(std::move(state)), m_entry(std::move(entry)) {}

            void reset() {
                if (m_entry) {
                    m_state->release(std::move(m_entry));
                    m_state.reset();
                }
            }

            std::shared_ptr<State> m_state;
            EntryPtr m_entry;
        };

        explicit LruCache(CacheOptions options = {}) : m_state(std::make_shared<State>(options)) {}

        ~LruCache() { clear(); }
        LruCache(const LruCache &) = delete;
        LruCache &operator=(const LruCache &) = delete;

        std::optional<Lease> acquire(const Key &key) {
            const std::lock_guard lock(m_state->mutex);
            const auto it = m_state->entries.find(key);
            if (it == m_state->entries.end()) {
                return {};
            }

            const auto &entry = it->second;
            ++entry->leases;
            entry->lastUsed = Clock::now();
            m_state->order.splice(m_state->order.begin(), m_state->order, entry->position);
            return Lease(m_state, entry);
        }

        Lease insert(Key key, Value value) {
            auto entry = std::make_shared<Entry>(Entry{std::move(key), std::move(value), Clock::now()});
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(m_state->mutex);
                if (const auto old = m_state->entries.find(entry->key); old != m_state->entries.end()) {
                    const auto previous = old->second;
                    m_state->remove(previous, retired);
                }

                m_state->order.push_front(entry);
                entry->position = m_state->order.begin();
                m_state->entries.emplace(entry->key, entry);
                m_state->trim(retired);
            }

            return Lease(m_state, std::move(entry));
        }

        void erase(const Key &key) {
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(m_state->mutex);
                if (const auto it = m_state->entries.find(key); it != m_state->entries.end()) {
                    const auto entry = it->second;
                    m_state->remove(entry, retired);
                }
            }
        }

        void sweep(Clock::time_point now = Clock::now()) {
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(m_state->mutex);
                if (m_state->options.idleTimeout.count() > 0) {
                    for (auto it = m_state->order.begin(); it != m_state->order.end();) {
                        const auto entry = *it++;
                        if (!entry->leases && std::chrono::duration_cast<std::chrono::milliseconds>(now - entry->lastUsed) >= m_state->options.idleTimeout) {
                            m_state->remove(entry, retired);
                        }
                    }
                }

                m_state->trim(retired);
            }
        }

        void clear() {
            std::vector<EntryPtr> retired;
            {
                const std::lock_guard lock(m_state->mutex);
                while (!m_state->order.empty()) {
                    const auto entry = m_state->order.front();
                    m_state->remove(entry, retired);
                }
            }
        }

        std::size_t size() const {
            const std::lock_guard lock(m_state->mutex);
            return m_state->entries.size();
        }

    private:
        std::shared_ptr<State> m_state;
    };
}

#endif // DSSP_RESOURCE_LRUCACHE_H

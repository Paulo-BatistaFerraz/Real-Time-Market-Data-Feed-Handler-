#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace qf::core {

// Type-erased base for subscriber lists
class SubscriberListBase {
public:
    virtual ~SubscriberListBase() = default;
};

// Typed subscriber list holding callbacks for a specific event type
template <typename EventType>
class SubscriberList : public SubscriberListBase {
public:
    using Callback = std::function<void(const EventType&)>;

    void add(Callback cb) { callbacks.push_back(std::move(cb)); }

    void dispatch(const EventType& event) const {
        for (const auto& cb : callbacks) {
            cb(event);
        }
    }

    const std::vector<Callback>& get_callbacks() const { return callbacks; }

private:
    std::vector<Callback> callbacks;
};

// Internal event bus — pub/sub with type-based dispatch.
//
// Thread safety model:
//   - subscribe() is called at startup under an exclusive lock
//   - publish() can be called from any thread under a shared lock
//   - This allows concurrent publishes with no contention
class EventBus {
public:
    EventBus() = default;

    // Register a callback for EventType. Call at startup before publishing.
    template <typename EventType>
    void subscribe(std::function<void(const EventType&)> callback) {
        std::unique_lock lock(mutex_);
        auto key = std::type_index(typeid(EventType));
        auto it = subscribers_.find(key);
        if (it == subscribers_.end()) {
            auto list = std::make_unique<SubscriberList<EventType>>();
            list->add(std::move(callback));
            subscribers_.emplace(key, std::move(list));
        } else {
            auto* list = static_cast<SubscriberList<EventType>*>(it->second.get());
            list->add(std::move(callback));
        }
    }

    // Dispatch event to all registered listeners for its type. Thread-safe.
    template <typename EventType>
    void publish(const EventType& event) const {
        std::shared_lock lock(mutex_);
        auto it = subscribers_.find(std::type_index(typeid(EventType)));
        if (it == subscribers_.end()) return;
        const auto* list = static_cast<const SubscriberList<EventType>*>(it->second.get());
        list->dispatch(event);
    }

    // Returns the number of subscribers for a given event type.
    template <typename EventType>
    size_t subscriber_count() const {
        std::shared_lock lock(mutex_);
        auto it = subscribers_.find(std::type_index(typeid(EventType)));
        if (it == subscribers_.end()) return 0;
        const auto* list = static_cast<const SubscriberList<EventType>*>(it->second.get());
        return list->get_callbacks().size();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::type_index, std::unique_ptr<SubscriberListBase>> subscribers_;
};

}  // namespace qf::core

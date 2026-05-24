#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

/**
 * @file EventBus.h
 * @brief Type-safe publish/subscribe event bus with RAII subscription guards.
 *
 * Provides decoupled communication between components. Events are typed C++ structs
 * dispatched synchronously. Subscriptions are managed via ScopedSubscription objects
 * that auto-unsubscribe on destruction.
 *
 * Usage:
 * @code
 * struct LevelStartEvent { int levelIndex; };
 *
 * EventBus bus;
 * auto sub = bus.Subscribe<LevelStartEvent>([](const LevelStartEvent& e) {
 *     // handle event
 * });
 * bus.Publish(LevelStartEvent{3}); // triggers handler
 * // sub goes out of scope → handler automatically removed
 * @endcode
 */

class EventBus;

/**
 * @class ScopedSubscription
 * @brief RAII guard that unsubscribes from an EventBus when destroyed.
 *
 * Move-only. Moving transfers ownership; the moved-from object becomes inert.
 */
class ScopedSubscription {
public:
    ScopedSubscription() = default;

    ~ScopedSubscription() { Unsubscribe(); }

    ScopedSubscription(ScopedSubscription &&other) noexcept
        : m_Bus(other.m_Bus), m_Type(other.m_Type), m_Id(other.m_Id) {
        other.m_Bus = nullptr;
        other.m_Id = 0;
    }

    ScopedSubscription &operator=(ScopedSubscription &&other) noexcept {
        if (this != &other) {
            Unsubscribe();
            m_Bus = other.m_Bus;
            m_Type = other.m_Type;
            m_Id = other.m_Id;
            other.m_Bus = nullptr;
            other.m_Id = 0;
        }
        return *this;
    }

    ScopedSubscription(const ScopedSubscription &) = delete;
    ScopedSubscription &operator=(const ScopedSubscription &) = delete;

    /** @brief Manually unsubscribe before destruction. Safe to call multiple times. */
    void Unsubscribe();

    /** @brief Returns true if this subscription is still active. */
    bool IsActive() const { return m_Bus != nullptr && m_Id != 0; }

private:
    friend class EventBus;

    ScopedSubscription(EventBus *bus, std::type_index type, uint64_t id)
        : m_Bus(bus), m_Type(type), m_Id(id) {}

    EventBus *m_Bus = nullptr;
    std::type_index m_Type = std::type_index(typeid(void));
    uint64_t m_Id = 0;
};

/**
 * @class EventBus
 * @brief Synchronous type-safe event dispatcher.
 *
 * Components subscribe to typed event structs and receive callbacks when
 * those events are published. All dispatch is synchronous on the calling thread.
 */
class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;

    /**
     * @brief Subscribe to events of type E.
     * @tparam E Event struct type.
     * @param handler Callback invoked with const E& when event is published.
     * @return ScopedSubscription RAII guard. Handler is removed when guard is destroyed.
     */
    template <typename E>
    ScopedSubscription Subscribe(std::function<void(const E &)> handler) {
        auto typeIdx = std::type_index(typeid(E));
        uint64_t id = ++m_NextId;

        auto wrapper = [handler = std::move(handler)](const void *data) {
            handler(*static_cast<const E *>(data));
        };

        m_Handlers[typeIdx].push_back({id, std::move(wrapper)});
        return ScopedSubscription(this, typeIdx, id);
    }

    /**
     * @brief Subscribe using a callable (lambda, functor) without wrapping in std::function.
     */
    template <typename E, typename F>
    ScopedSubscription Subscribe(F &&handler) {
        return Subscribe<E>(std::function<void(const E &)>(std::forward<F>(handler)));
    }

    /**
     * @brief Publish an event to all subscribers of type E.
     * @tparam E Event struct type.
     * @param event The event data to dispatch.
     *
     * Dispatch is synchronous. Subscribers are called in subscription order.
     * It is safe for a handler to unsubscribe during dispatch.
     */
    template <typename E>
    void Publish(const E &event) {
        auto typeIdx = std::type_index(typeid(E));
        auto it = m_Handlers.find(typeIdx);
        if (it == m_Handlers.end()) return;

        // Index-based iteration with size snapshot — avoids copying the vector.
        // Safe if handlers add/remove subscriptions during dispatch because we
        // re-check liveness and only iterate up to the original count.
        auto &vec = it->second;
        const size_t count = vec.size();
        for (size_t i = 0; i < count; ++i) {
            if (i < vec.size() && IsHandlerAlive(typeIdx, vec[i].id)) {
                vec[i].handler(&event);
            }
        }
    }

    /**
     * @brief Returns the number of active subscribers for event type E.
     */
    template <typename E>
    size_t SubscriberCount() const {
        auto it = m_Handlers.find(std::type_index(typeid(E)));
        return it != m_Handlers.end() ? it->second.size() : 0;
    }

    /** @brief Remove all subscriptions for all event types. */
    void Clear() {
        m_Handlers.clear();
    }

private:
    friend class ScopedSubscription;

    struct HandlerEntry {
        uint64_t id;
        std::function<void(const void *)> handler;
    };

    void RemoveHandler(std::type_index type, uint64_t id) {
        auto it = m_Handlers.find(type);
        if (it == m_Handlers.end()) return;

        auto &vec = it->second;
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                           [id](const HandlerEntry &e) { return e.id == id; }),
            vec.end());

        if (vec.empty()) {
            m_Handlers.erase(it);
        }
    }

    bool IsHandlerAlive(std::type_index type, uint64_t id) const {
        auto it = m_Handlers.find(type);
        if (it == m_Handlers.end()) return false;

        for (const auto &e : it->second) {
            if (e.id == id) return true;
        }
        return false;
    }

    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_Handlers;
    uint64_t m_NextId = 0;
};

// ScopedSubscription implementation (must be after EventBus definition)
inline void ScopedSubscription::Unsubscribe() {
    if (m_Bus && m_Id != 0) {
        m_Bus->RemoveHandler(m_Type, m_Id);
        m_Bus = nullptr;
        m_Id = 0;
    }
}

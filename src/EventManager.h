#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>

#include <sol/sol.hpp>

// Forward declare to avoid circular includes
class TASEngine;

/**
 * @class EventManager
 * @brief An event dispatcher that supports both C++ functions and Lua functions.
 */
class EventManager {
public:
    /**
     * @brief Callback variant that can hold either C++ function or Lua function
     */
    using Callback = std::variant<std::function<void()>, sol::function>;

    /**
     * @brief Wrapper for callback with one-time execution flag
     */
    struct CallbackEntry {
        Callback callback;
        bool oneTime = false;

        explicit CallbackEntry(Callback cb, bool once = false) : callback(std::move(cb)), oneTime(once) {}
    };

    explicit EventManager(TASEngine *engine);

    EventManager(const EventManager &) = delete;
    EventManager &operator=(const EventManager &) = delete;

    /**
     * @brief Register a Lua function to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The Lua function to call.
     * @param oneTime If true, the listener will be removed after first call.
     */
    void RegisterListener(const std::string &eventName, sol::function callback, bool oneTime = false);

    /**
     * @brief Register a C++ function to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ function to call (no parameters).
     * @param oneTime If true, the listener will be removed after first call.
     */
    void RegisterListener(const std::string &eventName, std::function<void()> callback, bool oneTime = false);

    /**
     * @brief Register a C++ lambda to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ lambda to call.
     * @param oneTime If true, the listener will be removed after first call.
     */
    template <typename F>
    void RegisterListener(const std::string &eventName, F &&callback, bool oneTime = false,
                          typename std::enable_if_t<
                              std::is_invocable_v<F> &&
                              !std::is_same_v<std::decay_t<F>, sol::function> &&
                              !std::is_same_v<std::decay_t<F>, std::function<void()>>
                          > * = nullptr);

    /**
     * @brief Register a one-time Lua function listener.
     * @param eventName The name of the event to listen for.
     * @param callback The Lua function to call.
     */
    void RegisterOnceListener(const std::string &eventName, sol::function callback);

    /**
     * @brief Register a one-time C++ function listener.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ function to call.
     */
    void RegisterOnceListener(const std::string &eventName, std::function<void()> callback);

    /**
     * @brief Register a one-time C++ lambda listener.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ lambda to call.
     */
    template <typename F>
    void RegisterOnceListener(const std::string &eventName, F &&callback,
                              typename std::enable_if_t<
                                  std::is_invocable_v<F> &&
                                  !std::is_same_v<std::decay_t<F>, sol::function> &&
                                  !std::is_same_v<std::decay_t<F>, std::function<void()>>
                              > * = nullptr);

    /**
     * @brief Fire an event with optional arguments to all registered listeners.
     * @param eventName The name of the event to fire.
     * @param args The arguments to pass to Lua listeners (C++ listeners ignore args).
     */
    template <typename... Args>
    void FireEvent(const std::string &eventName, Args &&... args);

    /**
     * @brief Clear all event listeners.
     */
    void ClearListeners();

    /**
     * @brief Clear listeners for a specific event.
     * @param eventName The event to clear listeners for.
     */
    void ClearListeners(const std::string &eventName);

    /**
     * @brief Get the number of listeners for an event.
     * @param eventName The event name.
     * @return The number of listeners.
     */
    size_t GetListenerCount(const std::string &eventName) const;

    /**
     * @brief Check if an event has any listeners.
     * @param eventName The event name.
     * @return True if there are listeners for this event.
     */
    bool HasListeners(const std::string &eventName) const;

private:
    /**
     * @brief Check if a callback is valid.
     * @param entry The callback entry to check.
     * @return True if the callback is valid.
     */
    static bool IsCallbackValid(const CallbackEntry &entry);

    /**
     * @brief Call a callback with optional arguments.
     * @param entry The callback entry to call.
     * @param args Arguments for Lua functions (ignored by C++ functions).
     * @return True if the call was successful.
     */
    template <typename... Args>
    bool CallCallback(const CallbackEntry &entry, Args &&... args) const;

    /**
     * @brief Handle errors in callbacks.
     * @param eventName The event name.
     * @param error The error message.
     */
    void HandleError(const std::string &eventName, const std::string &error) const;

    TASEngine *m_Engine;
    std::unordered_map<std::string, std::vector<CallbackEntry>> m_Listeners;
};

// Template implementation
template <typename F>
void EventManager::RegisterListener(const std::string &eventName, F &&callback, bool oneTime,
                                    typename std::enable_if_t<
                                        std::is_invocable_v<F> &&
                                        !std::is_same_v<std::decay_t<F>, sol::function> &&
                                        !std::is_same_v<std::decay_t<F>, std::function<void()>>
                                    > *) {
    if (eventName.empty()) {
        HandleError(eventName, "Event name cannot be empty");
        return;
    }

    // Convert lambda to std::function<void()>
    std::function<void()> wrappedCallback = [callback = std::forward<F>(callback)]() mutable {
        callback();
    };

    m_Listeners[eventName].emplace_back(std::move(wrappedCallback), oneTime);
}

template <typename F>
void EventManager::RegisterOnceListener(const std::string &eventName, F &&callback,
                                        typename std::enable_if_t<
                                            std::is_invocable_v<F> &&
                                            !std::is_same_v<std::decay_t<F>, sol::function> &&
                                            !std::is_same_v<std::decay_t<F>, std::function<void()>>
                                        > *) {
    RegisterListener(eventName, std::forward<F>(callback), true);
}

template <typename... Args>
void EventManager::FireEvent(const std::string &eventName, Args &&... args) {
    auto it = m_Listeners.find(eventName);
    if (it == m_Listeners.end()) {
        return; // No listeners
    }

    // Process all listeners
    for (auto listenerIt = it->second.begin(); listenerIt != it->second.end();) {
        CallbackEntry &entry = *listenerIt;

        if (!IsCallbackValid(entry)) {
            // Remove invalid callback
            listenerIt = it->second.erase(listenerIt);
            continue;
        }

        bool success = CallCallback(entry, std::forward<Args>(args)...);
        if (!success) {
            HandleError(eventName, "Callback execution failed");
        }

        // Remove one-time listeners after execution (whether successful or not)
        // This prevents failed one-time listeners from being called repeatedly
        if (entry.oneTime) {
            listenerIt = it->second.erase(listenerIt);
        } else {
            ++listenerIt;
        }
    }

    // Clean up empty event lists
    if (it->second.empty()) {
        m_Listeners.erase(it);
    }
}

template <typename... Args>
bool EventManager::CallCallback(const CallbackEntry &entry, Args &&... args) const {
    try {
        if (std::holds_alternative<sol::function>(entry.callback)) {
            // Lua function
            const auto &luaFunc = std::get<sol::function>(entry.callback);
            auto result = luaFunc(std::forward<Args>(args)...);
            return result.valid();
        } else {
            // C++ function (ignores arguments)
            const auto &cppFunc = std::get<std::function<void()>>(entry.callback);
            cppFunc();
            return true;
        }
    } catch (...) {
        // Catch all exceptions to ensure callback failures don't crash the event system
        return false;
    }
}
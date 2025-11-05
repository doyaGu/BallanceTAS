#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <atomic>
#include <cstdint>

#include <sol/sol.hpp>

/**
 * @class EventManager
 * @brief An event dispatcher that supports both C++ functions and Lua functions.
 *
 * LIFETIME SAFETY:
 * ================
 * EventManager stores Lua callbacks (sol::function) which hold references to a Lua VM.
 * To prevent use-after-free errors:
 *
 * 1. CRITICAL: ClearListeners() MUST be called before the Lua VM is destroyed.
 * 2. Each ScriptContext owns its own EventManager, ensuring proper cleanup.
 * 3. ScriptContext::Shutdown() ensures correct destruction order:
 *    - First: EventManager::ClearListeners() (clears Lua callbacks)
 *    - Then:  EventManager is destroyed
 *    - Finally: Lua VM is destroyed
 *
 * 4. Lua callback validity is checked before invocation (sol::function::valid()).
 * 5. Invalid callbacks are automatically removed during FireEvent().
 *
 * This design ensures Lua callbacks never outlive their associated Lua VM.
 */
class EventManager {
public:
    using ListenerId = uint64_t;
    static constexpr ListenerId kInvalidListenerId = 0;

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
        ListenerId id = kInvalidListenerId;

        CallbackEntry(ListenerId listenerId, Callback cb, bool once = false)
            : callback(std::move(cb)), oneTime(once), id(listenerId) {
        }
    };

    EventManager() = default;

    EventManager(const EventManager &) = delete;
    EventManager &operator=(const EventManager &) = delete;

    /**
     * @brief Register a Lua function to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The Lua function to call.
     * @param oneTime If true, the listener will be removed after first call.
     */
    ListenerId RegisterListener(const std::string &eventName, sol::function callback, bool oneTime = false);

    /**
     * @brief Register a C++ function to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ function to call (no parameters).
     * @param oneTime If true, the listener will be removed after first call.
     */
    ListenerId RegisterListener(const std::string &eventName, std::function<void()> callback, bool oneTime = false);

    /**
     * @brief Register a C++ lambda to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ lambda to call.
     * @param oneTime If true, the listener will be removed after first call.
     */
    template <typename F>
    ListenerId RegisterListener(const std::string &eventName, F &&callback, bool oneTime = false,
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
    ListenerId RegisterOnceListener(const std::string &eventName, sol::function callback);

    /**
     * @brief Register a one-time C++ function listener.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ function to call.
     */
    ListenerId RegisterOnceListener(const std::string &eventName, std::function<void()> callback);

    /**
     * @brief Register a one-time C++ lambda listener.
     * @param eventName The name of the event to listen for.
     * @param callback The C++ lambda to call.
     */
    template <typename F>
    ListenerId RegisterOnceListener(const std::string &eventName, F &&callback,
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
     * @brief Unregister a listener using its handle.
     * @param eventName The event name associated with the listener.
     * @param id The listener identifier returned by RegisterListener.
     * @return True if a listener was removed.
     */
    bool UnregisterListener(const std::string &eventName, ListenerId id);

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

    std::unordered_map<std::string, std::vector<CallbackEntry>> m_Listeners;
    std::atomic<ListenerId> m_NextListenerId{1};
};

// Template implementation
template <typename F>
EventManager::ListenerId EventManager::RegisterListener(const std::string &eventName, F &&callback, bool oneTime,
                                                        typename std::enable_if_t<
                                                            std::is_invocable_v<F> &&
                                                            !std::is_same_v<std::decay_t<F>, sol::function> &&
                                                            !std::is_same_v<std::decay_t<F>, std::function<void()>>
                                                        > *) {
    if (eventName.empty()) {
        HandleError(eventName, "Event name cannot be empty");
        return kInvalidListenerId;
    }

    // Convert lambda to std::function<void()>
    std::function<void()> wrappedCallback = [callback = std::forward<F>(callback)]() mutable {
        callback();
    };

    return RegisterListener(eventName, std::move(wrappedCallback), oneTime);
}

template <typename F>
EventManager::ListenerId EventManager::RegisterOnceListener(const std::string &eventName, F &&callback,
                                                            typename std::enable_if_t<
                                                                std::is_invocable_v<F> &&
                                                                !std::is_same_v<std::decay_t<F>, sol::function> &&
                                                                !std::is_same_v<std::decay_t<F>, std::function<void()>>
                                                            > *) {
    return RegisterListener(eventName, std::forward<F>(callback), true);
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

            // Double-check that the function is still valid before calling
            // Sol2 functions can become invalid if Lua state changes
            if (!luaFunc.valid()) {
                return false;
            }

            // Call the Lua function and check the result
            auto result = luaFunc(std::forward<Args>(args)...);

            // Check if the call was successful
            if (!result.valid()) {
                sol::error err = result;
                HandleError("lua_callback", std::string("Lua execution error: ") + err.what());
                return false;
            }

            return true;
        } else {
            // C++ function (ignores arguments)
            const auto &cppFunc = std::get<std::function<void()>>(entry.callback);
            if (!cppFunc) {
                HandleError("cpp_callback", "C++ function is null");
                return false;
            }
            cppFunc();
            return true;
        }
    } catch (const sol::error &e) {
        // Specific handling for Sol2 errors
        HandleError("lua_callback", std::string("Sol2 error: ") + e.what());
        return false;
    } catch (const std::exception &e) {
        // Handle other standard exceptions
        HandleError("callback", std::string("Exception: ") + e.what());
        return false;
    } catch (...) {
        // Catch all other exceptions
        HandleError("callback", "Unknown exception occurred");
        return false;
    }
}

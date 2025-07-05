#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sol/sol.hpp>

#include "LuaScheduler.h"

// Forward declare to avoid circular includes
class LuaScheduler;

/**
 * @class EventManager
 * @brief A central dispatcher for game events that can be subscribed to from Lua.
 */
class EventManager {
public:
    explicit EventManager(LuaScheduler &scheduler);

    EventManager(const EventManager &) = delete;
    EventManager &operator=(const EventManager &) = delete;

    /**
     * @brief Register a Lua function to be called when an event is fired.
     * @param eventName The name of the event to listen for.
     * @param callback The Lua function to call.
     */
    void RegisterListener(const std::string &eventName, sol::function callback);

    /**
     * @brief Fire an event with optional arguments to all registered listeners.
     * @param eventName The name of the event to fire.
     * @param args The arguments to pass to the event listeners.
     */
    template <typename... Args>
    void FireEvent(const std::string &eventName, Args... args) {
        auto it = m_Listeners.find(eventName);
        if (it == m_Listeners.end()) {
            return; // No one is listening to this event.
        }

        // Process listeners with error handling
        for (auto listenerIt = it->second.begin(); listenerIt != it->second.end();) {
            const auto &listenerFunc = *listenerIt;

            if (!listenerFunc.valid()) {
                // Remove invalid listeners
                listenerIt = it->second.erase(listenerIt);
                continue;
            }

            try {
                // Try to call the function directly first
                auto result = listenerFunc(args...);

                if (result.valid()) {
                    // Function executed successfully without yielding
                    ++listenerIt;
                } else {
                    // Handle error
                    sol::error err = result;
                    HandleLuaError(eventName, err.what());
                    ++listenerIt;
                }
            } catch (const sol::error &) {
                // This might be thrown if the function yields
                // For event listeners that need to yield, start them as separate coroutines
                try {
                    sol::coroutine co(listenerFunc);
                    auto co_result = co(args...);

                    if (!co_result.valid()) {
                        sol::error err = co_result;
                        HandleLuaError(eventName, err.what());
                    } else if (co.runnable()) {
                        // Coroutine yielded, add it to the scheduler
                        m_Scheduler.StartCoroutine(co);
                    }
                    ++listenerIt;
                } catch (const std::exception &inner_e) {
                    HandleLuaError(eventName, inner_e.what());
                    ++listenerIt;
                }
            } catch (const std::exception &e) {
                HandleLuaError(eventName, e.what());
                ++listenerIt;
            }
        }

        // Clean up empty event lists
        if (it->second.empty()) {
            m_Listeners.erase(it);
        }
    }

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

private:
    /**
     * @brief Handle Lua errors in event callbacks.
     * @param eventName The name of the event that caused the error.
     * @param error The error message.
     */
    void HandleLuaError(const std::string &eventName, const std::string &error);

    LuaScheduler &m_Scheduler;
    std::unordered_map<std::string, std::vector<sol::function>> m_Listeners;
};

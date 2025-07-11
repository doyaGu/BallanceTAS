#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sol/sol.hpp>

// Forward declare to avoid circular includes
class TASEngine;

/**
 * @class EventManager
 * @brief A central dispatcher for game events that can be subscribed to from Lua.
 */
class EventManager {
public:
    explicit EventManager(TASEngine *engine);

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
    void FireEvent(const std::string &eventName, Args... args);

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

    TASEngine *m_Engine;
    std::unordered_map<std::string, std::vector<sol::function>> m_Listeners;
};
#include "EventManager.h"

#include "Logger.h"

EventManager::ListenerId EventManager::RegisterListener(const std::string &eventName, sol::function callback, bool oneTime) {
    if (eventName.empty()) {
        HandleError(eventName, "Event name cannot be empty");
        return kInvalidListenerId;
    }

    if (!callback.valid()) {
        HandleError(eventName, "Lua callback is invalid");
        return kInvalidListenerId;
    }

    ListenerId id = m_NextListenerId.fetch_add(1, std::memory_order_relaxed);
    m_Listeners[eventName].emplace_back(id, std::move(callback), oneTime);
    return id;
}

EventManager::ListenerId EventManager::RegisterListener(const std::string &eventName, std::function<void()> callback, bool oneTime) {
    if (eventName.empty()) {
        HandleError(eventName, "Event name cannot be empty");
        return kInvalidListenerId;
    }

    if (!callback) {
        HandleError(eventName, "C++ callback is invalid");
        return kInvalidListenerId;
    }

    ListenerId id = m_NextListenerId.fetch_add(1, std::memory_order_relaxed);
    m_Listeners[eventName].emplace_back(id, std::move(callback), oneTime);
    return id;
}

EventManager::ListenerId EventManager::RegisterOnceListener(const std::string &eventName, sol::function callback) {
    return RegisterListener(eventName, std::move(callback), true);
}

EventManager::ListenerId EventManager::RegisterOnceListener(const std::string &eventName, std::function<void()> callback) {
    return RegisterListener(eventName, std::move(callback), true);
}

void EventManager::ClearListeners() {
    // CRITICAL: This must be called before the Lua VM is destroyed.
    // Sol2 function destructors may access the Lua VM during cleanup.
    // ScriptContext::Shutdown() ensures correct destruction order.
    try {
        m_Listeners.clear();
    } catch (const std::exception &e) {
        // Log error but don't throw - we're likely in a cleanup path
        Log::Error("EventManager::ClearListeners: Exception during cleanup: %s", e.what());
    }
}

void EventManager::ClearListeners(const std::string &eventName) {
    try {
        auto it = m_Listeners.find(eventName);
        if (it != m_Listeners.end()) {
            m_Listeners.erase(it);
        }
    } catch (const std::exception &e) {
        // Log error but don't throw
        Log::Error("EventManager::ClearListeners('%s'): Exception during cleanup: %s",
                   eventName.c_str(), e.what());
    }
}

size_t EventManager::GetListenerCount(const std::string &eventName) const {
    auto it = m_Listeners.find(eventName);
    return it != m_Listeners.end() ? it->second.size() : 0;
}

bool EventManager::HasListeners(const std::string &eventName) const {
    auto it = m_Listeners.find(eventName);
    return it != m_Listeners.end() && !it->second.empty();
}

bool EventManager::UnregisterListener(const std::string &eventName, ListenerId id) {
    if (id == kInvalidListenerId) {
        return false;
    }

    auto it = m_Listeners.find(eventName);
    if (it == m_Listeners.end()) {
        return false;
    }

    auto &listeners = it->second;
    for (auto listenerIt = listeners.begin(); listenerIt != listeners.end(); ++listenerIt) {
        if (listenerIt->id == id) {
            listeners.erase(listenerIt);
            if (listeners.empty()) {
                m_Listeners.erase(it);
            }
            return true;
        }
    }

    return false;
}

bool EventManager::IsCallbackValid(const CallbackEntry& entry) {
    if (std::holds_alternative<sol::function>(entry.callback)) {
        return std::get<sol::function>(entry.callback).valid();
    } else {
        return static_cast<bool>(std::get<std::function<void()>>(entry.callback));
    }
}

void EventManager::HandleError(const std::string &eventName, const std::string &error) const {
    Log::Error("Error in event '%s': %s", eventName.c_str(), error.c_str());
}
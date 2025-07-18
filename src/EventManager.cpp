#include "EventManager.h"

#include "TASEngine.h"

EventManager::EventManager(TASEngine *engine) : m_Engine(engine) {}

void EventManager::RegisterListener(const std::string &eventName, sol::function callback, bool oneTime) {
    if (eventName.empty()) {
        HandleError(eventName, "Event name cannot be empty");
        return;
    }

    if (!callback.valid()) {
        HandleError(eventName, "Lua callback is invalid");
        return;
    }

    m_Listeners[eventName].emplace_back(std::move(callback), oneTime);
}

void EventManager::RegisterListener(const std::string &eventName, std::function<void()> callback, bool oneTime) {
    if (eventName.empty()) {
        HandleError(eventName, "Event name cannot be empty");
        return;
    }

    if (!callback) {
        HandleError(eventName, "C++ callback is invalid");
        return;
    }

    m_Listeners[eventName].emplace_back(std::move(callback), oneTime);
}

void EventManager::RegisterOnceListener(const std::string &eventName, sol::function callback) {
    RegisterListener(eventName, std::move(callback), true);
}

void EventManager::RegisterOnceListener(const std::string &eventName, std::function<void()> callback) {
    RegisterListener(eventName, std::move(callback), true);
}

void EventManager::ClearListeners() {
    m_Listeners.clear();
}

void EventManager::ClearListeners(const std::string &eventName) {
    auto it = m_Listeners.find(eventName);
    if (it != m_Listeners.end()) {
        m_Listeners.erase(it);
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

bool EventManager::IsCallbackValid(const CallbackEntry& entry) {
    if (std::holds_alternative<sol::function>(entry.callback)) {
        return std::get<sol::function>(entry.callback).valid();
    } else {
        return static_cast<bool>(std::get<std::function<void()>>(entry.callback));
    }
}

void EventManager::HandleError(const std::string &eventName, const std::string &error) const {
    if (m_Engine && m_Engine->GetLogger()) {
        m_Engine->GetLogger()->Error("Error in event '%s': %s", eventName.c_str(), error.c_str());
    }
}
#include "EventManager.h"
#include "LuaScheduler.h"

EventManager::EventManager(LuaScheduler &scheduler) : m_Scheduler(scheduler) {}

void EventManager::RegisterListener(const std::string &eventName, sol::function callback) {
    if (!callback.valid() || eventName.empty()) {
        return;
    }

    m_Listeners[eventName].push_back(std::move(callback));
}

void EventManager::ClearListeners() {
    m_Listeners.clear();
}

void EventManager::ClearListeners(const std::string &eventName) {
    auto it = m_Listeners.find(eventName);
    if (it != m_Listeners.end()) {
        it->second.clear();
    }
}

size_t EventManager::GetListenerCount(const std::string &eventName) const {
    auto it = m_Listeners.find(eventName);
    return it != m_Listeners.end() ? it->second.size() : 0;
}

void EventManager::HandleLuaError(const std::string &eventName, const std::string &error) {
    // TODO: Route to proper error handling system
    fprintf(stderr, "[EventManager] Error in event '%s': %s\n", eventName.c_str(), error.c_str());

    // Could also fire an error event or show UI modal here
    // m_Engine->GetUIManager()->ShowError("Event Error", error);
}

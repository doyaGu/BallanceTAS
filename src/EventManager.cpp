#include "EventManager.h"

#include "TASEngine.h"

EventManager::EventManager(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::invalid_argument("TASEngine cannot be null");
    }
}

void EventManager::RegisterListener(const std::string &eventName, sol::function callback) {
    if (!callback.valid() || eventName.empty()) {
        return;
    }

    m_Listeners[eventName].push_back(std::move(callback));
}

template <typename... Args>
void EventManager::FireEvent(const std::string &eventName, Args... args) {
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

template void EventManager::FireEvent(const std::string &);
template void EventManager::FireEvent(const std::string &, int);

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
    m_Engine->GetLogger()->Error("Error in event '%s': %s\n", eventName.c_str(), error.c_str());
}

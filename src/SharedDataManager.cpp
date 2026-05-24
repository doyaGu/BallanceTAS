#include "SharedDataManager.h"

#include <stdexcept>
#include <chrono>

#include "Logger.h"
#include "TASEngine.h"

namespace {

std::string TrimSharedKey(const std::string &key) {
    const size_t first = key.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const size_t last = key.find_last_not_of(" \t\r\n");
    return key.substr(first, last - first + 1);
}

} // namespace

SharedDataManager::SharedDataManager(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("SharedDataManager requires a valid TASEngine instance.");
    }
}

SharedDataManager::~SharedDataManager() {
    Shutdown();
}

bool SharedDataManager::Initialize() {
    if (m_IsInitialized) {
        Log::Warn("SharedDataManager already initialized.");
        return true;
    }

    Log::Info("Initializing SharedDataManager...");

    try {
        // Initialize mutex and clear any existing data
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Data.clear();

        m_IsInitialized = true;
        Log::Info("SharedDataManager initialized successfully.");
        return true;
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize SharedDataManager: %s", e.what());
        return false;
    }
}

void SharedDataManager::Shutdown() {
    if (!m_IsInitialized) return;

    Log::Info("Shutting down SharedDataManager...");

    try {
        Clear();
        m_IsInitialized = false;
        Log::Info("SharedDataManager shutdown complete.");
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("Exception during SharedDataManager shutdown: %s", e.what());
        }
    }
}

bool SharedDataManager::Set(const std::string &key, const tas::lua::LuaValue &value, const SetOptions &options) {
    if (key.empty()) {
        Log::Warn("SharedDataManager: Cannot set value with empty key.");
        return false;
    }

    try {
        std::lock_guard<std::mutex> lock(m_Mutex);

        // Get old value for watch notifications
        StoredValue oldValue;
        auto it = m_Data.find(key);
        if (it != m_Data.end()) {
            oldValue = it->second;
        }

        // Calculate expiry time if TTL is set
        int64_t expiryTime = 0;
        if (options.ttl_ms > 0) {
            expiryTime = GetCurrentTimeMs() + options.ttl_ms;
        }

        StoredValue newValue(value, expiryTime);

        // Store the value
        m_Data[key] = newValue;

        // Queue watch notification for delivery on next Tick() (avoids mutex deadlock)
        QueueWatchNotificationLocked(key, oldValue, newValue);

        return true;
    } catch (const std::exception &e) {
        Log::Error("SharedDataManager: Failed to set key '%s': %s",
                                    key.c_str(), e.what());
        return false;
    }
}

tas::lua::LuaValue SharedDataManager::Get(const std::string &key, const tas::lua::LuaValue &defaultValue) {
    if (key.empty()) {
        Log::Warn("SharedDataManager: Cannot get value with empty key.");
        return defaultValue;
    }

    try {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Data.find(key);
        if (it != m_Data.end()) {
            // Check if value has expired
            if (it->second.IsExpired(GetCurrentTimeMs())) {
                // Value expired, remove it and return default
                StoredValue expiredValue = it->second;
                QueueWatchNotificationLocked(key, expiredValue, StoredValue());
                m_Data.erase(it);
                return defaultValue;
            }
            return it->second.value;
        }
        return defaultValue;
    } catch (const std::exception &e) {
        Log::Error("SharedDataManager: Failed to get key '%s': %s",
                                    key.c_str(), e.what());
        return defaultValue;
    }
}

bool SharedDataManager::Has(const std::string &key) const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Data.find(key);
    if (it == m_Data.end()) {
        return false;
    }

    int64_t currentTime = GetCurrentTimeMs();
    if (it->second.IsExpired(currentTime)) {
        return false;
    }

    return true;
}

bool SharedDataManager::Remove(const std::string &key) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Data.find(key);
    if (it == m_Data.end()) {
        return false;
    }

    StoredValue oldValue = it->second;
    m_Data.erase(it);
    QueueWatchNotificationLocked(key, oldValue, StoredValue());
    return true;
}

void SharedDataManager::Clear() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (const auto &[key, value] : m_Data) {
        QueueWatchNotificationLocked(key, value, StoredValue());
    }
    m_Data.clear();
}

void SharedDataManager::ClearNamespace(const std::string &prefix) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto it = m_Data.begin(); it != m_Data.end();) {
        if (it->first.rfind(prefix, 0) == 0) {
            QueueWatchNotificationLocked(it->first, it->second, StoredValue());
            it = m_Data.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::string> SharedDataManager::GetKeys() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    std::vector<std::string> keys;
    keys.reserve(m_Data.size());
    int64_t currentTime = GetCurrentTimeMs();
    for (const auto &[key, value] : m_Data) {
        if (!value.IsExpired(currentTime)) {
            keys.push_back(key);
        }
    }
    return keys;
}

size_t SharedDataManager::GetSize() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    int64_t currentTime = GetCurrentTimeMs();
    size_t count = 0;
    for (const auto &[key, value] : m_Data) {
        if (!value.IsExpired(currentTime)) {
            ++count;
        }
    }
    return count;
}

std::string SharedDataManager::MakeGlobalKey(const std::string &key) {
    return "global:" + TrimSharedKey(key);
}

std::string SharedDataManager::MakeSharedKey(const std::string &key) {
    return "shared:" + TrimSharedKey(key);
}

// ============================================================================
// Watch Management
// ============================================================================

void SharedDataManager::Watch(const std::string &contextName, std::weak_ptr<ScriptContext> contextPtr,
                              const std::string &key, tas::lua::LuaFunction callback) {
    if (contextName.empty() || key.empty() || !callback.IsValid()) {
        Log::Warn("[%s] SharedDataManager: Invalid watch parameters (context: %s, key: %s, callback valid: %s).",
                                   contextName.empty() ? "unknown" : contextName.c_str(),
                                   contextName.empty() ? "empty" : contextName.c_str(),
                                   key.empty() ? "empty" : key.c_str(),
                                   callback.IsValid() ? "yes" : "no");
        return;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Watches[key][contextName] = WatchEntry(contextPtr, std::move(callback), ++m_WatchGeneration);
    Log::Info("[%s] Watching key '%s' (generation: %llu).",
                               contextName.c_str(), key.c_str(), m_WatchGeneration);
}

void SharedDataManager::Unwatch(const std::string &contextName, const std::string &key) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_Watches.find(key);
    if (it != m_Watches.end()) {
        it->second.erase(contextName);
        if (it->second.empty()) {
            m_Watches.erase(it);
        }
    }
}

void SharedDataManager::UnwatchAll(const std::string &contextName) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto it = m_Watches.begin(); it != m_Watches.end();) {
        it->second.erase(contextName);
        if (it->second.empty()) {
            it = m_Watches.erase(it);
        } else {
            ++it;
        }
    }
}

void SharedDataManager::TriggerWatches(const std::string &key,
                                      const StoredValue &oldValue,
                                      const StoredValue &newValue) {
    // Note: This should be called WITHOUT holding the mutex to avoid deadlocks
    // Callbacks are invoked on the game thread during Tick()

    // Step 1: Copy watch entries while holding mutex (avoid race condition)
    std::unordered_map<std::string, WatchEntry> watchEntries;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Watches.find(key);
        if (it == m_Watches.end()) {
            return;  // No watches for this key
        }
        watchEntries = it->second;  // Copy the watch entries map
    }

    // Step 2: Invoke all callbacks outside mutex, validating context lifetime
    for (const auto &[contextName, entry] : watchEntries) {
        // Validate context is still alive
        auto contextPtr = entry.context.lock();
        if (entry.trackContextLifetime && !contextPtr) {
            // Context has been destroyed, skip this callback
            Log::Warn("SharedDataManager: Watch callback skipped for destroyed context '%s' (key: %s)",
                                       contextName.c_str(), key.c_str());
            continue;
        }

        if (!entry.callback || !entry.callback->IsValid()) {
            continue;
        }

        try {
            auto result = entry.callback->Call(3, 0, [&](lua_State *state) {
                newValue.value.Push(state);
                oldValue.value.Push(state);
                lua_pushlstring(state, key.data(), key.size());
            });
            if (result.IsError()) {
                Log::Error("SharedDataManager: Watch callback error (%s, %s): %s",
                                           contextName.c_str(), key.c_str(), result.GetError().message.c_str());
            }
        } catch (const std::exception &e) {
            Log::Error("SharedDataManager: Exception in watch callback (%s, %s): %s",
                                       contextName.c_str(), key.c_str(), e.what());
        }
    }
}

void SharedDataManager::QueueWatchNotificationLocked(const std::string &key,
                                                    const StoredValue &oldValue,
                                                    const StoredValue &newValue) {
    auto it = m_Watches.find(key);
    if (it == m_Watches.end()) {
        return;
    }

    m_PendingNotifications.push_back({key, oldValue, newValue});
}

// ============================================================================
// TTL Management
// ============================================================================

void SharedDataManager::Tick() {
    // Step 1: Collect pending notifications accumulated during Set/Remove/etc.
    std::vector<WatchNotification> notifications;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        notifications = std::move(m_PendingNotifications);
        m_PendingNotifications.clear();
    }

    // Step 2: Process TTL expiration and gather additional notifications
    std::vector<std::string> expiredKeys;
    std::vector<WatchNotification> ttlNotifications;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        int64_t currentTime = GetCurrentTimeMs();

        for (auto it = m_Data.begin(); it != m_Data.end();) {
            if (it->second.IsExpired(currentTime)) {
                expiredKeys.push_back(it->first);

                auto watchIt = m_Watches.find(it->first);
                if (watchIt != m_Watches.end()) {
                    ttlNotifications.push_back({it->first, it->second, StoredValue()});
                }

                it = m_Data.erase(it);
            } else {
                ++it;
            }
        }
    }

    notifications.insert(notifications.end(), ttlNotifications.begin(), ttlNotifications.end());

    // Step 3: Invoke watch callbacks outside mutex to avoid deadlocks
    for (const auto &notif : notifications) {
        TriggerWatches(notif.key, notif.oldValue, notif.newValue);
    }

    // Step 4: Log expired keys after processing
    for (const auto &key : expiredKeys) {
        Log::Info("SharedDataManager: Key '%s' expired, removing.", key.c_str());
    }
}

int64_t SharedDataManager::GetCurrentTimeMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

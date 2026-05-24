#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

#include "LuaRuntime/LuaFunction.h"
#include "LuaRuntime/LuaValue.h"

// Forward declarations
class TASEngine;
class ScriptContext;

/**
 * @class SharedDataManager
 * @brief Manages shared data across script contexts.
 *
 * This class provides a thread-safe key-value store that can be accessed
 * from all script contexts. It supports Lua values and provides serialization
 * for cross-context data sharing.
 *
 * Lua API:
 *   tas.shared.set(key, value)
 *   tas.shared.get(key, default)
 *   tas.shared.has(key)
 *   tas.shared.remove(key)
 *   tas.shared.clear()
 *   tas.shared.keys()
 */
class SharedDataManager {
public:
    explicit SharedDataManager(TASEngine *engine);
    ~SharedDataManager();

    // SharedDataManager is not copyable or movable
    SharedDataManager(const SharedDataManager &) = delete;
    SharedDataManager &operator=(const SharedDataManager &) = delete;

    /**
     * @brief Initializes the shared data manager.
     * @return True if initialization was successful.
     */
    bool Initialize();

    /**
     * @brief Shuts down the shared data manager and clears all data.
     */
    void Shutdown();

    /**
     * @brief Options for setting shared data.
     */
    struct SetOptions {
        explicit SetOptions(int ttl = 0) : ttl_ms(ttl) {}
        int ttl_ms; // Time-to-live in milliseconds (0 = no expiry)
    };

    /**
     * @brief Sets a shared value with optional TTL.
     * @param key The key to store the value under.
     * @param value The portable Lua value to store.
     * @param options Optional settings (TTL, etc.).
     * @return True if the value was stored successfully.
     */
    bool Set(const std::string &key, const tas::lua::LuaValue &value, const SetOptions &options = SetOptions());

    /**
     * @brief Gets a shared value.
     * @param key The key to retrieve.
     * @param defaultValue Optional default value if key doesn't exist.
     * @return The stored value, or defaultValue if not found.
     */
    tas::lua::LuaValue Get(const std::string &key, const tas::lua::LuaValue &defaultValue = tas::lua::LuaValue());

    /**
     * @brief Checks if a key exists in shared data.
     * @param key The key to check.
     * @return True if the key exists.
     */
    bool Has(const std::string &key) const;

    /**
     * @brief Removes a key from shared data.
     * @param key The key to remove.
     * @return True if the key existed and was removed.
     */
    bool Remove(const std::string &key);

    /**
     * @brief Clears all shared data.
     */
    void Clear();
    void ClearNamespace(const std::string &prefix);

    /**
     * @brief Gets all keys in shared data.
     * @return Vector of all keys.
     */
    std::vector<std::string> GetKeys() const;

    /**
     * @brief Gets the number of entries in shared data.
     * @return Number of entries.
     */
    size_t GetSize() const;

    static std::string MakeGlobalKey(const std::string &key);
    static std::string MakeSharedKey(const std::string &key);

    /**
     * @brief Watches a key for changes and calls callback when value changes.
     * @param contextName Name of the context registering the watch.
     * @param contextPtr Weak pointer to the context (for lifetime tracking).
     * @param key The key to watch.
     * @param callback Lua function to call when value changes (receives new_value, old_value, key).
     */
    void Watch(const std::string &contextName, std::weak_ptr<ScriptContext> contextPtr,
               const std::string &key, tas::lua::LuaFunction callback);

    /**
     * @brief Removes a watch for a specific key in a context.
     * @param contextName Name of the context.
     * @param key The key to stop watching.
     */
    void Unwatch(const std::string &contextName, const std::string &key);

    /**
     * @brief Removes all watches for a context.
     * @param contextName Name of the context.
     */
    void UnwatchAll(const std::string &contextName);

    /**
     * @brief Processes TTL expiration and triggers change notifications.
     * Should be called once per tick.
     */
    void Tick();

private:
    /**
     * @brief Represents a watch entry with context lifetime tracking.
     */
    struct WatchEntry {
        std::weak_ptr<ScriptContext> context; // Weak pointer to track context lifetime
        std::shared_ptr<tas::lua::LuaFunction> callback; // Lua callback function
        uint64_t generation;                  // Generation counter for versioning
        bool trackContextLifetime = false;

        WatchEntry() : generation(0) {}

        WatchEntry(std::weak_ptr<ScriptContext> ctx, tas::lua::LuaFunction cb, uint64_t gen)
            : context(std::move(ctx)),
              callback(std::make_shared<tas::lua::LuaFunction>(std::move(cb))),
              generation(gen),
              trackContextLifetime(!context.expired()) {}
    };

    /**
     * @brief Represents a stored value with its type information.
     */
    struct StoredValue {
        tas::lua::LuaValue value;
        int64_t expiryTime = 0; // 0 = no expiry, otherwise milliseconds since epoch

        StoredValue() = default;

        StoredValue(tas::lua::LuaValue v, int64_t expiry = 0)
            : value(std::move(v)), expiryTime(expiry) {}

        // Check if value has expired
        bool IsExpired(int64_t currentTime) const {
            return expiryTime > 0 && currentTime >= expiryTime;
        }
    };

    /**
     * @brief Gets current time in milliseconds since epoch.
     * @return Current time in milliseconds.
     */
    static int64_t GetCurrentTimeMs();

    /**
     * @brief Triggers watch callbacks for a key change.
     * @param key The key that changed.
     * @param oldValue The old value (can be nil).
     * @param newValue The new value.
     */
    void TriggerWatches(const std::string &key, const StoredValue &oldValue, const StoredValue &newValue);

    void QueueWatchNotificationLocked(const std::string &key,
                                      const StoredValue &oldValue,
                                      const StoredValue &newValue);

    // Core references
    TASEngine *m_Engine;

    // Thread-safe storage
    mutable std::mutex m_Mutex;
    std::unordered_map<std::string, StoredValue> m_Data;

    // Watch callbacks: key -> (contextName -> WatchEntry)
    std::unordered_map<std::string, std::unordered_map<std::string, WatchEntry>> m_Watches;
    uint64_t m_WatchGeneration = 0; // Global generation counter for watch versioning

    // Pending watch notifications (queued for delivery on Tick())
    struct WatchNotification {
        std::string key;
        StoredValue oldValue;
        StoredValue newValue;
    };

    std::vector<WatchNotification> m_PendingNotifications;

    // Initialization state
    bool m_IsInitialized = false;
};

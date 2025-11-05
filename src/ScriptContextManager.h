#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <unordered_map>

#include "ScriptContext.h"

// Forward declarations
class TASEngine;
class SharedDataManager;
class MessageBus;

/**
 * @brief Configuration for context pool
 */
struct ContextPoolConfig {
    size_t maxPoolSize = 4;           // Maximum contexts in pool
    bool enablePooling = true;        // Enable pooling feature
    int hibernateFrameThreshold = 60; // Frames of inactivity before hibernation
};

/**
 * @brief Configuration for custom context creation
 */
struct CustomContextLimits {
    size_t maxTotalCustomContexts = 10;         // Max total custom contexts
    size_t maxCustomContextsPerLevel = 5;       // Max custom contexts per level
    size_t memoryLimitBytes = 10 * 1024 * 1024; // 10MB per custom context
};

/**
 * @class ScriptContextManager
 * @brief Manages multiple script execution contexts.
 *
 * This class is responsible for:
 * - Creating and destroying script contexts
 * - Managing context lifecycle (global and level contexts)
 * - Ticking all active contexts in priority order
 * - Providing inter-context communication (shared data, messages)
 */
class ScriptContextManager {
public:
    explicit ScriptContextManager(TASEngine *engine);
    ~ScriptContextManager();

    // ScriptContextManager is not copyable or movable
    ScriptContextManager(const ScriptContextManager &) = delete;
    ScriptContextManager &operator=(const ScriptContextManager &) = delete;

    /**
     * @brief Initializes the context manager.
     * @return True if initialization was successful.
     */
    bool Initialize();

    /**
     * @brief Shuts down the context manager and all contexts.
     */
    void Shutdown();

    /**
     * @brief Creates a new script context.
     * @param name Unique name for the context.
     * @param type Type of the context.
     * @param priority Priority for input and event handling (higher = more priority).
     * @return Shared pointer to the created context, or nullptr if creation failed.
     */
    std::shared_ptr<ScriptContext> CreateContext(const std::string &name,
                                                 ScriptContextType type,
                                                 int priority = 0);

    /**
     * @brief Gets an existing context by name.
     * @param name Name of the context to retrieve.
     * @return Shared pointer to the context, or nullptr if not found.
     */
    std::shared_ptr<ScriptContext> GetContext(const std::string &name);

    /**
     * @brief Gets an existing context by name (const version).
     * @param name Name of the context to retrieve.
     * @return Shared pointer to the context, or nullptr if not found.
     */
    std::shared_ptr<const ScriptContext> GetContext(const std::string &name) const;

    /**
     * @brief Destroys a context by name.
     * @param name Name of the context to destroy.
     * @return True if the context was found and destroyed.
     */
    bool DestroyContext(const std::string &name);

    /**
     * @brief Destroys all contexts of a specific type.
     * @param type Type of contexts to destroy.
     */
    void DestroyContextsByType(ScriptContextType type);

    /**
     * @brief Gets the global context (creates it if it doesn't exist).
     * @return Shared pointer to the global context.
     */
    std::shared_ptr<ScriptContext> GetOrCreateGlobalContext();

    /**
     * @brief Gets the level context for the current level.
     * @param levelName Name of the level (used to generate context name).
     * @return Shared pointer to the level context, or nullptr if not found.
     */
    std::shared_ptr<ScriptContext> GetLevelContext(const std::string &levelName);

    /**
     * @brief Creates or retrieves the level context for the current level.
     * @param levelName Name of the level.
     * @return Shared pointer to the level context.
     */
    std::shared_ptr<ScriptContext> GetOrCreateLevelContext(const std::string &levelName);

    /**
     * @brief Destroys all level contexts.
     */
    void DestroyAllLevelContexts();

    /**
     * @brief Processes one tick for all active contexts (in priority order).
     */
    void TickAll();

    /**
     * @brief Checks if any context is currently executing.
     * @return True if at least one context is executing.
     */
    bool IsAnyContextExecuting() const;

    /**
     * @brief Gets all contexts sorted by priority (highest first).
     * @return Vector of shared pointers to contexts sorted by priority.
     */
    std::vector<std::shared_ptr<ScriptContext>> GetContextsByPriority();

    /**
     * @brief Gets all contexts sorted by priority (const version).
     * @return Vector of shared pointers to contexts sorted by priority.
     */
    std::vector<std::shared_ptr<const ScriptContext>> GetContextsByPriority() const;

    /**
     * @brief Gets all contexts (alias for GetContextsByPriority).
     * @return Vector of all shared pointers to contexts.
     */
    std::vector<std::shared_ptr<ScriptContext>> GetAllContexts() { return GetContextsByPriority(); }

    /**
     * @brief Gets all contexts (const version, alias for GetContextsByPriority).
     * @return Vector of all shared pointers to contexts.
     */
    std::vector<std::shared_ptr<const ScriptContext>> GetAllContexts() const { return GetContextsByPriority(); }

    /**
     * @brief Gets the shared data manager for inter-context communication.
     * @return Pointer to the shared data manager.
     */
    SharedDataManager *GetSharedDataManager() const { return m_SharedData.get(); }

    /**
     * @brief Alias for GetSharedDataManager.
     * @return Pointer to the shared data manager.
     */
    SharedDataManager *GetSharedData() const { return GetSharedDataManager(); }

    /**
     * @brief Gets the message bus for inter-context communication.
     * @return Pointer to the message bus.
     */
    MessageBus *GetMessageBus() const { return m_MessageBus.get(); }

    /**
     * @brief Gets the number of active contexts.
     * @return Number of contexts.
     */
    size_t GetContextCount() const { return m_Contexts.size(); }

    // --- VM Pooling ---

    /**
     * @brief Acquires a context from the pool or creates a new one.
     * @param type The type of context to acquire.
     * @param name The name for the context.
     * @param priority The priority for the context.
     * @return Shared pointer to the acquired context.
     */
    std::shared_ptr<ScriptContext> AcquirePooledContext(ScriptContextType type, const std::string &name, int priority);

    /**
     * @brief Releases a context back to the pool or destroys it.
     * @param context The context to release.
     * @return True if the context was pooled, false if destroyed.
     */
    bool ReleaseOrPoolContext(ScriptContext *context);

    /**
     * @brief Gets the pool configuration.
     * @return Reference to the pool configuration.
     */
    ContextPoolConfig &GetPoolConfig() { return m_PoolConfig; }

    /**
     * @brief Gets the pool configuration (const version).
     * @return Const reference to the pool configuration.
     */
    const ContextPoolConfig &GetPoolConfig() const { return m_PoolConfig; }

    /**
     * @brief Sets the pool configuration.
     * @param config The new pool configuration.
     */
    void SetPoolConfig(const ContextPoolConfig &config) { m_PoolConfig = config; }

    // --- Custom Context Management ---

    /**
     * @brief Creates a custom context with limits and quotas.
     * @param name Unique name for the custom context.
     * @param priority Priority for the context.
     * @param limits Resource limits for the context.
     * @return Shared pointer to the created context, or nullptr if limits exceeded.
     */
    std::shared_ptr<ScriptContext> CreateCustomContext(const std::string &name, int priority, const CustomContextLimits &limits);

    /**
     * @brief Gets the custom context limits.
     * @return Reference to the custom context limits.
     */
    CustomContextLimits &GetCustomContextLimits() { return m_CustomLimits; }

    /**
     * @brief Gets the custom context limits (const version).
     * @return Const reference to the custom context limits.
     */
    const CustomContextLimits &GetCustomContextLimits() const { return m_CustomLimits; }

    // --- Event Subscription ---

    /**
     * @brief Subscribes a context to a game event.
     * @param contextName Name of the context to subscribe.
     * @param eventName Name of the event to subscribe to.
     */
    void SubscribeToEvent(const std::string &contextName, const std::string &eventName);

    /**
     * @brief Unsubscribes a context from a game event.
     * @param contextName Name of the context to unsubscribe.
     * @param eventName Name of the event to unsubscribe from.
     */
    void UnsubscribeFromEvent(const std::string &contextName, const std::string &eventName);

    /**
     * @brief Unsubscribes a context from all events.
     * @param contextName Name of the context.
     */
    void UnsubscribeFromAllEvents(const std::string &contextName);

    /**
     * @brief Checks if a context is subscribed to an event.
     * @param contextName Name of the context.
     * @param eventName Name of the event.
     * @return True if the context is subscribed to the event.
     */
    bool IsSubscribedToEvent(const std::string &contextName, const std::string &eventName) const;

    /**
     * @brief Fires a game event to all contexts.
     * @param eventName The name of the event.
     * @param args Optional arguments to pass to event handlers.
     */
    template <typename... Args>
    void FireGameEventToAll(const std::string &eventName, Args... args);

    /**
     * @brief Fires a game event to a specific context.
     * @param contextName Name of the context to fire the event to.
     * @param eventName The name of the event.
     * @param args Optional arguments to pass to event handlers.
     */
    template <typename... Args>
    void FireGameEventToContext(const std::string &contextName, const std::string &eventName, Args... args);

private:
    /**
     * @brief Generates a unique context name for a level.
     * @param levelName Name of the level.
     * @return Unique context name.
     */
    static std::string GenerateLevelContextName(const std::string &levelName);

    std::string GetCurrentLevelKey() const;
    void RegisterCustomContext(const std::string &name, const std::string &levelKey, size_t memoryLimitBytes);
    void UnregisterCustomContext(const std::string &name);

    // Core references
    TASEngine *m_Engine;

    // Context storage (name -> context)
    std::map<std::string, std::shared_ptr<ScriptContext>> m_Contexts;

    // Inter-context communication
    std::unique_ptr<SharedDataManager> m_SharedData;
    std::unique_ptr<MessageBus> m_MessageBus;

    // VM Pooling (LRU)
    struct PooledContext {
        std::shared_ptr<ScriptContext> context;
        ScriptContextType type;
        size_t lastUsedTick;
    };

    std::vector<PooledContext> m_ContextPool;
    ContextPoolConfig m_PoolConfig;

    // Custom context management
    CustomContextLimits m_CustomLimits;
    size_t m_CustomContextCount = 0;
    std::unordered_map<std::string, size_t> m_CustomContextsPerLevel;
    std::unordered_map<std::string, std::string> m_CustomContextLevelMap;
    std::unordered_map<std::string, size_t> m_CustomContextMemoryLimits;

    // Event subscriptions (eventName -> set of contextNames)
    std::map<std::string, std::vector<std::string>> m_EventSubscriptions;

    // Initialization state
    bool m_IsInitialized = false;
};

// Template implementations
template <typename... Args>
void ScriptContextManager::FireGameEventToAll(const std::string &eventName, Args... args) {
    // Fire event only to subscribed contexts (subscription-based routing)
    auto it = m_EventSubscriptions.find(eventName);
    if (it != m_EventSubscriptions.end()) {
        for (const auto &contextName : it->second) {
            auto context = GetContext(contextName);
            if (context && context->IsExecuting()) {
                context->FireGameEvent(eventName, args...);
            }
        }
    }
}

template <typename... Args>
void ScriptContextManager::FireGameEventToContext(const std::string &contextName, const std::string &eventName, Args... args) {
    auto context = GetContext(contextName);
    if (context && context->IsExecuting()) {
        context->FireGameEvent(eventName, args...);
    }
}

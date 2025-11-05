#include "ScriptContextManager.h"

#include "Logger.h"
#include "TASEngine.h"
#include "ScriptContext.h"
#include "SharedDataManager.h"
#include "MessageBus.h"
#include "GameInterface.h"
#include <algorithm>

namespace {
    constexpr const char *kGlobalCustomContextKey = "__global__";
}

ScriptContextManager::ScriptContextManager(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("ScriptContextManager requires a valid TASEngine instance.");
    }
}

ScriptContextManager::~ScriptContextManager() {
    Shutdown();
}

bool ScriptContextManager::Initialize() {
    if (m_IsInitialized) {
        Log::Warn("ScriptContextManager already initialized.");
        return true;
    }

    Log::Info("Initializing ScriptContextManager...");

    try {
        // Initialize shared data manager
        m_SharedData = std::make_unique<SharedDataManager>(m_Engine);
        if (!m_SharedData->Initialize()) {
            Log::Error("Failed to initialize SharedDataManager.");
            return false;
        }

        // Initialize message bus
        m_MessageBus = std::make_unique<MessageBus>(m_Engine);
        if (!m_MessageBus->Initialize()) {
            Log::Error("Failed to initialize MessageBus.");
            return false;
        }

        m_IsInitialized = true;
        Log::Info("ScriptContextManager initialized successfully.");
        return true;
    } catch (const std::exception &e) {
        Log::Error("Failed to initialize ScriptContextManager: %s", e.what());
        return false;
    }
}

void ScriptContextManager::Shutdown() {
    if (!m_IsInitialized) return;

    Log::Info("Shutting down ScriptContextManager...");

    try {
        // Destroy all contexts
        m_Contexts.clear();
        m_ContextPool.clear();
        m_CustomContextsPerLevel.clear();
        m_CustomContextLevelMap.clear();
        m_CustomContextMemoryLimits.clear();
        m_CustomContextCount = 0;

        // Shutdown message bus
        if (m_MessageBus) {
            m_MessageBus->Shutdown();
            m_MessageBus.reset();
        }

        // Shutdown shared data manager
        if (m_SharedData) {
            m_SharedData->Shutdown();
            m_SharedData.reset();
        }

        m_IsInitialized = false;
        Log::Info("ScriptContextManager shutdown complete.");
    } catch (const std::exception &e) {
        if (m_Engine) {
            Log::Error("Exception during ScriptContextManager shutdown: %s", e.what());
        }
    }
}

std::shared_ptr<ScriptContext> ScriptContextManager::CreateContext(const std::string &name,
                                                                   ScriptContextType type,
                                                                   int priority) {
    if (name.empty()) {
        Log::Error("Cannot create context with empty name.");
        return nullptr;
    }

    // Check if context already exists
    if (m_Contexts.find(name) != m_Contexts.end()) {
        Log::Warn("Context '%s' already exists.", name.c_str());
        return m_Contexts[name];
    }

    try {
        Log::Info("Creating script context '%s' (priority: %d)...", name.c_str(), priority);

        // Create new context
        auto context = std::make_shared<ScriptContext>(m_Engine, name, type, priority);

        // Initialize the context
        if (!context->Initialize()) {
            Log::Error("Failed to initialize context '%s'.", name.c_str());
            return nullptr;
        }

        // Store the context (shared ownership)
        m_Contexts[name] = context;

        Log::Info("Script context '%s' created successfully.", name.c_str());
        return context;
    } catch (const std::exception &e) {
        Log::Error("Exception creating context '%s': %s", name.c_str(), e.what());
        return nullptr;
    }
}

std::shared_ptr<ScriptContext> ScriptContextManager::GetContext(const std::string &name) {
    auto it = m_Contexts.find(name);
    if (it != m_Contexts.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<const ScriptContext> ScriptContextManager::GetContext(const std::string &name) const {
    auto it = m_Contexts.find(name);
    if (it != m_Contexts.end()) {
        return it->second;
    }
    return nullptr;
}

bool ScriptContextManager::DestroyContext(const std::string &name) {
    auto it = m_Contexts.find(name);
    if (it == m_Contexts.end()) {
        Log::Warn("Cannot destroy context '%s': not found.", name.c_str());
        return false;
    }

    Log::Info("Destroying script context '%s'...", name.c_str());

    try {
        // Clean up event subscriptions for this context
        UnsubscribeFromAllEvents(name);

        // Decrement custom context count if it's a custom context
        if (it->second && it->second->GetType() == ScriptContextType::Custom) {
            if (m_CustomContextCount > 0) {
                m_CustomContextCount--;
            }
        }

        UnregisterCustomContext(name);

        // Shutdown the context
        if (it->second) {
            it->second->Shutdown();
        }

        // Remove from map
        m_Contexts.erase(it);

        Log::Info("Script context '%s' destroyed.", name.c_str());
        return true;
    } catch (const std::exception &e) {
        Log::Error("Exception destroying context '%s': %s", name.c_str(), e.what());
        return false;
    }
}

void ScriptContextManager::DestroyContextsByType(ScriptContextType type) {
    Log::Info("Destroying all contexts of specified type...");

    // Collect context names to destroy (can't erase while iterating)
    std::vector<std::string> toDestroy;
    for (const auto &[name, context] : m_Contexts) {
        if (context && context->GetType() == type) {
            toDestroy.push_back(name);
        }
    }

    // Destroy collected contexts
    for (const auto &name : toDestroy) {
        DestroyContext(name);
    }

    Log::Info("Destroyed %zu contexts.", toDestroy.size());
}

std::shared_ptr<ScriptContext> ScriptContextManager::GetOrCreateGlobalContext() {
    static const std::string globalContextName = "global";
    static const int globalContextPriority = 0; // Lower priority than level contexts

    auto context = GetContext(globalContextName);
    if (context) {
        return context;
    }

    return CreateContext(globalContextName, ScriptContextType::Global, globalContextPriority);
}

std::shared_ptr<ScriptContext> ScriptContextManager::GetLevelContext(const std::string &levelName) {
    std::string contextName = GenerateLevelContextName(levelName);
    return GetContext(contextName);
}

std::shared_ptr<ScriptContext> ScriptContextManager::GetOrCreateLevelContext(const std::string &levelName) {
    std::string contextName = GenerateLevelContextName(levelName);
    static constexpr int levelContextPriority = 100; // Higher priority than global context

    auto context = GetContext(contextName);
    if (context) {
        return context;
    }

    return CreateContext(contextName, ScriptContextType::Level, levelContextPriority);
}

void ScriptContextManager::DestroyAllLevelContexts() {
    DestroyContextsByType(ScriptContextType::Level);
}

void ScriptContextManager::TickAll() {
    if (!m_IsInitialized) return;

    try {
        // Process shared data TTL and watch notifications first
        if (m_SharedData) {
            m_SharedData->Tick();
        }

        // Process message bus second (deliver pending messages)
        if (m_MessageBus) {
            m_MessageBus->ProcessMessages();
        }

        // Get contexts sorted by priority (highest first)
        auto contexts = GetContextsByPriority();

        std::vector<std::string> contextsToDestroy;
        contextsToDestroy.reserve(contexts.size());

        for (const auto &context : contexts) {
            if (!context || !context->IsExecuting()) {
                continue;
            }

            auto limitIt = m_CustomContextMemoryLimits.find(context->GetName());
            if (limitIt != m_CustomContextMemoryLimits.end()) {
                const size_t usage = context->GetLuaMemoryBytes();
                if (usage > limitIt->second) {
                    Log::Warn(
                        "Custom context '%s' exceeded memory limit (%zu / %zu bytes). Destroying context.",
                        context->GetName().c_str(), usage, limitIt->second
                    );
                    context->Stop();
                    contextsToDestroy.push_back(context->GetName());
                    continue;
                }
            }

            context->Tick();
        }

        for (const auto &name : contextsToDestroy) {
            DestroyContext(name);
        }
    } catch (const std::exception &e) {
        Log::Error("Exception during ScriptContextManager tick: %s", e.what());
    }
}

bool ScriptContextManager::IsAnyContextExecuting() const {
    for (const auto &[name, context] : m_Contexts) {
        if (context && context->IsExecuting()) {
            return true;
        }
    }
    return false;
}

std::vector<std::shared_ptr<ScriptContext>> ScriptContextManager::GetContextsByPriority() {
    std::vector<std::shared_ptr<ScriptContext>> contexts;
    contexts.reserve(m_Contexts.size());

    for (auto &[name, context] : m_Contexts) {
        if (context) {
            contexts.push_back(context);
        }
    }

    // Sort by priority (highest first)
    std::sort(contexts.begin(), contexts.end(),
              [](const std::shared_ptr<ScriptContext> &a, const std::shared_ptr<ScriptContext> &b) {
                  return a->GetPriority() > b->GetPriority();
              });

    return contexts;
}

std::vector<std::shared_ptr<const ScriptContext>> ScriptContextManager::GetContextsByPriority() const {
    std::vector<std::shared_ptr<const ScriptContext>> contexts;
    contexts.reserve(m_Contexts.size());

    for (const auto &[name, context] : m_Contexts) {
        if (context) {
            contexts.push_back(context);
        }
    }

    // Sort by priority (highest first)
    std::sort(contexts.begin(), contexts.end(),
              [](const std::shared_ptr<const ScriptContext> &a, const std::shared_ptr<const ScriptContext> &b) {
                  return a->GetPriority() > b->GetPriority();
              });

    return contexts;
}

std::string ScriptContextManager::GenerateLevelContextName(const std::string &levelName) {
    return "level_" + levelName;
}

std::string ScriptContextManager::GetCurrentLevelKey() const {
    if (!m_Engine) {
        return std::string(kGlobalCustomContextKey);
    }

    auto *gameInterface = m_Engine->GetGameInterface();
    if (!gameInterface) {
        return std::string(kGlobalCustomContextKey);
    }

    const std::string &mapName = gameInterface->GetMapName();
    if (mapName.empty()) {
        return std::string(kGlobalCustomContextKey);
    }

    return mapName;
}

void ScriptContextManager::RegisterCustomContext(const std::string &name, const std::string &levelKey, size_t memoryLimitBytes) {
    auto insertResult = m_CustomContextLevelMap.emplace(name, levelKey);
    if (insertResult.second) {
        m_CustomContextsPerLevel[levelKey]++;
    } else if (insertResult.first->second != levelKey) {
        const std::string previousKey = insertResult.first->second;
        auto prevCountIt = m_CustomContextsPerLevel.find(previousKey);
        if (prevCountIt != m_CustomContextsPerLevel.end()) {
            if (prevCountIt->second > 1) {
                prevCountIt->second--;
            } else {
                m_CustomContextsPerLevel.erase(prevCountIt);
            }
        }

        insertResult.first->second = levelKey;
        m_CustomContextsPerLevel[levelKey]++;
    }

    if (memoryLimitBytes > 0) {
        m_CustomContextMemoryLimits[name] = memoryLimitBytes;
    } else {
        m_CustomContextMemoryLimits.erase(name);
    }
}

void ScriptContextManager::UnregisterCustomContext(const std::string &name) {
    auto levelIt = m_CustomContextLevelMap.find(name);
    if (levelIt != m_CustomContextLevelMap.end()) {
        auto perLevelIt = m_CustomContextsPerLevel.find(levelIt->second);
        if (perLevelIt != m_CustomContextsPerLevel.end()) {
            if (perLevelIt->second > 1) {
                perLevelIt->second--;
            } else {
                m_CustomContextsPerLevel.erase(perLevelIt);
            }
        }

        m_CustomContextLevelMap.erase(levelIt);
    }

    m_CustomContextMemoryLimits.erase(name);
}

// ============================================================================
// Event Subscription Management
// ============================================================================

void ScriptContextManager::SubscribeToEvent(const std::string &contextName, const std::string &eventName) {
    if (contextName.empty() || eventName.empty()) {
        Log::Warn("Cannot subscribe with empty context or event name.");
        return;
    }

    // Check if context exists
    if (!GetContext(contextName)) {
        Log::Warn("Cannot subscribe: context '%s' does not exist.", contextName.c_str());
        return;
    }

    // Add to subscription list (avoid duplicates)
    auto &subscribers = m_EventSubscriptions[eventName];
    if (std::find(subscribers.begin(), subscribers.end(), contextName) == subscribers.end()) {
        subscribers.push_back(contextName);
        Log::Info("Context '%s' subscribed to event '%s'.",
                  contextName.c_str(), eventName.c_str());
    }
}

void ScriptContextManager::UnsubscribeFromEvent(const std::string &contextName, const std::string &eventName) {
    auto it = m_EventSubscriptions.find(eventName);
    if (it != m_EventSubscriptions.end()) {
        auto &subscribers = it->second;
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), contextName), subscribers.end());

        // Clean up empty subscription lists
        if (subscribers.empty()) {
            m_EventSubscriptions.erase(it);
        }
    }
}

void ScriptContextManager::UnsubscribeFromAllEvents(const std::string &contextName) {
    for (auto it = m_EventSubscriptions.begin(); it != m_EventSubscriptions.end();) {
        auto &subscribers = it->second;
        subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), contextName), subscribers.end());

        // Clean up empty subscription lists
        if (subscribers.empty()) {
            it = m_EventSubscriptions.erase(it);
        } else {
            ++it;
        }
    }
}

bool ScriptContextManager::IsSubscribedToEvent(const std::string &contextName, const std::string &eventName) const {
    auto it = m_EventSubscriptions.find(eventName);
    if (it != m_EventSubscriptions.end()) {
        const auto &subscribers = it->second;
        return std::find(subscribers.begin(), subscribers.end(), contextName) != subscribers.end();
    }
    return false;
}

// ============================================================================
// VM Pooling (LRU)
// ============================================================================

std::shared_ptr<ScriptContext> ScriptContextManager::AcquirePooledContext(ScriptContextType type, const std::string &name, int priority) {
    if (!m_PoolConfig.enablePooling) {
        // Pooling disabled, create new context directly
        return CreateContext(name, type, priority);
    }

    // Try to find a matching context in the pool
    for (auto it = m_ContextPool.begin(); it != m_ContextPool.end(); ++it) {
        if (it->type == type) {
            Log::Info("Reusing pooled context for '%s' (type: %d).",
                      name.c_str(), static_cast<int>(type));

            // Move context out of pool
            auto context = std::move(it->context);
            m_ContextPool.erase(it);

            // Reinitialize context with new name/priority (preserves expensive Lua VM)
            if (!context->Reinitialize(name, priority)) {
                Log::Warn("Failed to reinitialize pooled context, creating new one.");
                return CreateContext(name, type, priority);
            }

            // Re-register context with new name
            m_Contexts[name] = context;

            return context;
        }
    }

    // No matching context in pool, create new
    return CreateContext(name, type, priority);
}

bool ScriptContextManager::ReleaseOrPoolContext(ScriptContext *context) {
    if (!context || !m_PoolConfig.enablePooling) {
        return false; // Not pooled, will be destroyed
    }

    // Check if pool is full
    if (m_ContextPool.size() >= m_PoolConfig.maxPoolSize) {
        // Pool is full, evict LRU (oldest)
        if (!m_ContextPool.empty()) {
            auto oldestIt = std::min_element(m_ContextPool.begin(), m_ContextPool.end(),
                                             [](const PooledContext &a, const PooledContext &b) {
                                                 return a.lastUsedTick < b.lastUsedTick;
                                             });

            Log::Info("Pool full, evicting context (last used: %zu).", oldestIt->lastUsedTick);
            m_ContextPool.erase(oldestIt);
        }
    }

    // Find the context in active contexts and move to pool
    std::string contextName = context->GetName();
    auto it = m_Contexts.find(contextName);
    if (it != m_Contexts.end()) {
        // Stop the context (but don't destroy)
        context->Stop();

        if (context->GetType() == ScriptContextType::Custom && m_CustomContextCount > 0) {
            m_CustomContextCount--;
        }
        UnregisterCustomContext(contextName);

        // Move to pool
        PooledContext pooled;
        pooled.context = std::move(it->second);
        pooled.type = context->GetType();
        pooled.lastUsedTick = m_Engine->GetCurrentTick();

        m_ContextPool.push_back(std::move(pooled));
        m_Contexts.erase(it);

        Log::Info("Context '%s' moved to pool (pool size: %zu).",
                  contextName.c_str(), m_ContextPool.size());
        return true;
    }

    return false;
}

// ============================================================================
// Custom Context Management
// ============================================================================

std::shared_ptr<ScriptContext> ScriptContextManager::CreateCustomContext(const std::string &name, int priority, const CustomContextLimits &limits) {
    // Check total custom context limit
    if (m_CustomContextCount >= limits.maxTotalCustomContexts) {
        Log::Warn("Cannot create custom context '%s': total limit reached (%zu).",
                  name.c_str(), limits.maxTotalCustomContexts);
        return nullptr;
    }

    const std::string levelKey = GetCurrentLevelKey();
    const std::string levelLabel = (levelKey == kGlobalCustomContextKey) ? "global" : levelKey;

    if (limits.maxCustomContextsPerLevel > 0) {
        size_t perLevelCount = 0;
        auto it = m_CustomContextsPerLevel.find(levelKey);
        if (it != m_CustomContextsPerLevel.end()) {
            perLevelCount = it->second;
        }

        if (perLevelCount >= limits.maxCustomContextsPerLevel) {
            Log::Warn("Cannot create custom context '%s': per-level limit reached for '%s' (%zu).",
                      name.c_str(), levelLabel.c_str(), limits.maxCustomContextsPerLevel);
            return nullptr;
        }
    }

    // Create the custom context
    auto contextPtr = CreateContext(name, ScriptContextType::Custom, priority);
    if (contextPtr) {
        m_CustomContextCount++;
        RegisterCustomContext(name, levelKey, limits.memoryLimitBytes);
        size_t perLevelCount = m_CustomContextsPerLevel[levelKey];

        Log::Info(
            "Created custom context '%s' (total custom: %zu, %s contexts: %zu).",
            name.c_str(),
            m_CustomContextCount,
            levelLabel.c_str(),
            perLevelCount
        );
    }

    return contextPtr;
}

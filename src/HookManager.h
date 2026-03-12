#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

class CKTimeManager;
class CKInputManager;
class CKBaseManager;
class HookManager;

/// RAII guard that automatically removes a hook callback on destruction.
/// Move-only. If the HookManager is destroyed first, behavior is undefined.
class ScopedCallback {
public:
    ScopedCallback() = default;
    ~ScopedCallback() { Reset(); }

    ScopedCallback(ScopedCallback &&other) noexcept
        : m_Manager(other.m_Manager), m_Id(other.m_Id) {
        other.m_Manager = nullptr;
        other.m_Id = 0;
    }

    ScopedCallback &operator=(ScopedCallback &&other) noexcept {
        if (this != &other) {
            Reset();
            m_Manager = other.m_Manager;
            m_Id = other.m_Id;
            other.m_Manager = nullptr;
            other.m_Id = 0;
        }
        return *this;
    }

    ScopedCallback(const ScopedCallback &) = delete;
    ScopedCallback &operator=(const ScopedCallback &) = delete;

    bool IsValid() const { return m_Manager != nullptr && m_Id != 0; }
    void Reset();

private:
    friend class HookManager;
    ScopedCallback(HookManager *manager, uint64_t id)
        : m_Manager(manager), m_Id(id) {}

    HookManager *m_Manager = nullptr;
    uint64_t m_Id = 0;
};

/// Centralized manager for CKTimeManager and CKInputManager PreProcess hooks.
///
/// Replaces the scattered AddCallback/ClearCallbacks pattern with RAII-guarded
/// individual callback registration. Each Register method returns a ScopedCallback
/// that automatically unregisters when destroyed.
///
/// Callbacks must not throw exceptions (matching existing codebase convention).
///
/// Usage:
///   auto guard = hookMgr->RegisterPostTickCallback([](CKTimeManager *tm) {
///       tm->SetLastDeltaTime(delta);
///   });
///   // callback fires every tick until 'guard' is destroyed
class HookManager {
public:
    using TimeCallback = std::function<void(CKTimeManager *)>;
    using InputCallback = std::function<void(CKInputManager *)>;

    HookManager() = default;
    ~HookManager();

    HookManager(const HookManager &) = delete;
    HookManager &operator=(const HookManager &) = delete;

    /// Enable the CKTimeManager PreProcess hook. Call once at startup.
    bool EnableTimeManagerHook(CKBaseManager *timeManager);

    /// Enable the CKInputManager PreProcess hook. Call once at startup.
    bool EnableInputManagerHook(CKBaseManager *inputManager);

    /// Disable all hooks and clear all registered callbacks.
    void DisableAll();

    /// Register a callback that fires before CKTimeManager::PreProcess.
    [[nodiscard]] ScopedCallback RegisterPreTickCallback(TimeCallback callback);

    /// Register a callback that fires after CKTimeManager::PreProcess.
    [[nodiscard]] ScopedCallback RegisterPostTickCallback(TimeCallback callback);

    /// Register a callback that fires before CKInputManager::PreProcess.
    [[nodiscard]] ScopedCallback RegisterPreInputCallback(InputCallback callback);

    /// Register a callback that fires after CKInputManager::PreProcess.
    [[nodiscard]] ScopedCallback RegisterPostInputCallback(InputCallback callback);

private:
    friend class ScopedCallback;

    enum class Slot { PreTick, PostTick, PreInput, PostInput };

    void Remove(uint64_t id);
    void DoRemove(uint64_t id);
    void FlushPendingRemovals();
    uint64_t NextId() { return ++m_NextId; }

    uint64_t m_NextId = 0;
    bool m_TimeHookEnabled = false;
    bool m_InputHookEnabled = false;
    bool m_Dispatching = false;

    // Ordered by ID (monotonically increasing) for deterministic dispatch order
    std::map<uint64_t, TimeCallback> m_PreTickCallbacks;
    std::map<uint64_t, TimeCallback> m_PostTickCallbacks;
    std::map<uint64_t, InputCallback> m_PreInputCallbacks;
    std::map<uint64_t, InputCallback> m_PostInputCallbacks;

    std::unordered_map<uint64_t, Slot> m_IdToSlot;
    std::vector<uint64_t> m_PendingRemovals;
};

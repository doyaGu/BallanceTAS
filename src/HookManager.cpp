#include "HookManager.h"
#include "TASHook.h"

// ---------------------------------------------------------------------------
// ScopedCallback
// ---------------------------------------------------------------------------

void ScopedCallback::Reset() {
    if (m_Manager && m_Id != 0) {
        m_Manager->Remove(m_Id);
    }
    m_Manager = nullptr;
    m_Id = 0;
}

// ---------------------------------------------------------------------------
// HookManager
// ---------------------------------------------------------------------------

HookManager::~HookManager() {
    DisableAll();
}

bool HookManager::EnableTimeManagerHook(CKBaseManager *timeManager) {
    if (m_TimeHookEnabled)
        return true;

    if (!CKTimeManagerHook::Enable(timeManager))
        return false;

    m_TimeHookEnabled = true;

    // Install master dispatchers — these iterate our managed callback maps.
    CKTimeManagerHook::AddPreCallback([this](CKTimeManager *man) {
        m_Dispatching = true;
        for (auto &[id, cb] : m_PreTickCallbacks) {
            cb(man);
        }
        m_Dispatching = false;
        FlushPendingRemovals();
    });

    CKTimeManagerHook::AddPostCallback([this](CKTimeManager *man) {
        m_Dispatching = true;
        for (auto &[id, cb] : m_PostTickCallbacks) {
            cb(man);
        }
        m_Dispatching = false;
        FlushPendingRemovals();
    });

    return true;
}

bool HookManager::EnableInputManagerHook(CKBaseManager *inputManager) {
    if (m_InputHookEnabled)
        return true;

    if (!CKInputManagerHook::Enable(inputManager))
        return false;

    m_InputHookEnabled = true;

    CKInputManagerHook::AddPreCallback([this](CKBaseManager *man) {
        auto *inputMan = static_cast<CKInputManager *>(man);
        m_Dispatching = true;
        for (auto &[id, cb] : m_PreInputCallbacks) {
            cb(inputMan);
        }
        m_Dispatching = false;
        FlushPendingRemovals();
    });

    CKInputManagerHook::AddPostCallback([this](CKBaseManager *man) {
        auto *inputMan = static_cast<CKInputManager *>(man);
        m_Dispatching = true;
        for (auto &[id, cb] : m_PostInputCallbacks) {
            cb(inputMan);
        }
        m_Dispatching = false;
        FlushPendingRemovals();
    });

    return true;
}

void HookManager::DisableAll() {
    if (m_TimeHookEnabled) {
        CKTimeManagerHook::ClearPreCallbacks();
        CKTimeManagerHook::ClearPostCallbacks();
        CKTimeManagerHook::Disable();
        m_TimeHookEnabled = false;
    }

    if (m_InputHookEnabled) {
        CKInputManagerHook::ClearPreCallbacks();
        CKInputManagerHook::ClearPostCallbacks();
        CKInputManagerHook::Disable();
        m_InputHookEnabled = false;
    }

    m_PreTickCallbacks.clear();
    m_PostTickCallbacks.clear();
    m_PreInputCallbacks.clear();
    m_PostInputCallbacks.clear();
    m_IdToSlot.clear();
    m_PendingRemovals.clear();
}

ScopedCallback HookManager::RegisterPreTickCallback(TimeCallback callback) {
    auto id = NextId();
    m_PreTickCallbacks.emplace(id, std::move(callback));
    m_IdToSlot.emplace(id, Slot::PreTick);
    return {this, id};
}

ScopedCallback HookManager::RegisterPostTickCallback(TimeCallback callback) {
    auto id = NextId();
    m_PostTickCallbacks.emplace(id, std::move(callback));
    m_IdToSlot.emplace(id, Slot::PostTick);
    return {this, id};
}

ScopedCallback HookManager::RegisterPreInputCallback(InputCallback callback) {
    auto id = NextId();
    m_PreInputCallbacks.emplace(id, std::move(callback));
    m_IdToSlot.emplace(id, Slot::PreInput);
    return {this, id};
}

ScopedCallback HookManager::RegisterPostInputCallback(InputCallback callback) {
    auto id = NextId();
    m_PostInputCallbacks.emplace(id, std::move(callback));
    m_IdToSlot.emplace(id, Slot::PostInput);
    return {this, id};
}

void HookManager::Remove(uint64_t id) {
    if (m_Dispatching) {
        m_PendingRemovals.push_back(id);
        return;
    }
    DoRemove(id);
}

void HookManager::DoRemove(uint64_t id) {
    auto it = m_IdToSlot.find(id);
    if (it == m_IdToSlot.end())
        return;

    switch (it->second) {
    case Slot::PreTick:  m_PreTickCallbacks.erase(id);  break;
    case Slot::PostTick: m_PostTickCallbacks.erase(id);  break;
    case Slot::PreInput: m_PreInputCallbacks.erase(id);  break;
    case Slot::PostInput:m_PostInputCallbacks.erase(id); break;
    }
    m_IdToSlot.erase(it);
}

void HookManager::FlushPendingRemovals() {
    for (auto id : m_PendingRemovals) {
        DoRemove(id);
    }
    m_PendingRemovals.clear();
}

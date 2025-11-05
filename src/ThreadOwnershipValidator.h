#pragma once

#include <thread>
#include <atomic>
#include <cassert>
#include <string>

/**
 * ThreadOwnershipValidator - Enforces single-threaded access to components
 *
 * Usage:
 *   class MyComponent {
 *       ThreadOwnershipValidator m_ThreadValidator{"MyComponent"};
 *   public:
 *       void Method() {
 *           m_ThreadValidator.AssertOwnership();
 *           // ... implementation
 *       }
 *   };
 *
 * In debug builds, violations trigger assertions.
 * In release builds, the overhead is minimal (single atomic check).
 */
class ThreadOwnershipValidator {
public:
    explicit ThreadOwnershipValidator(const char *componentName)
        : m_ComponentName(componentName), m_OwnerThread(), m_IsInitialized(false) {}

    /**
     * Validates that the current thread owns this component.
     * On first call, captures the calling thread as the owner.
     * On subsequent calls, asserts that the caller is the owner thread.
     */
    void AssertOwnership() const {
        std::thread::id currentThread = std::this_thread::get_id();

        // Fast path: check if we're already initialized and on correct thread
        if (m_IsInitialized.load(std::memory_order_acquire)) {
            std::thread::id owner = m_OwnerThread.load(std::memory_order_acquire);
            if (currentThread != owner) {
                OnViolation(currentThread, owner);
            }
            return;
        }

        // Slow path: first-time initialization
        InitializeOwner(currentThread);
    }

    /**
     * Explicitly set the owner thread (e.g., during ownership transfer).
     * Use with caution - this is primarily for initialization scenarios.
     */
    void SetOwner(std::thread::id newOwner) {
        m_OwnerThread.store(newOwner, std::memory_order_release);
        m_IsInitialized.store(true, std::memory_order_release);
    }

    /**
     * Reset ownership, allowing a new thread to take ownership.
     * Should only be called when no other threads are accessing the component.
     */
    void ResetOwnership() {
        m_IsInitialized.store(false, std::memory_order_release);
    }

    /**
     * Check if current thread owns this component without asserting.
     * Returns true if owned by current thread, false otherwise.
     */
    bool IsOwnedByCurrentThread() const {
        if (!m_IsInitialized.load(std::memory_order_acquire)) {
            return false;
        }
        std::thread::id owner = m_OwnerThread.load(std::memory_order_acquire);
        return std::this_thread::get_id() == owner;
    }

    const char *GetComponentName() const { return m_ComponentName; }

private:
    void InitializeOwner(std::thread::id currentThread) const {
        // Try to claim ownership
        bool expected = false;
        if (m_IsInitialized.compare_exchange_strong(expected, true,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
            // We won the race - set ourselves as owner
            m_OwnerThread.store(currentThread, std::memory_order_release);
        } else {
            // Someone else initialized first - verify we're the same thread
            std::thread::id owner = m_OwnerThread.load(std::memory_order_acquire);
            if (currentThread != owner) {
                OnViolation(currentThread, owner);
            }
        }
    }

    [[noreturn]] void OnViolation(std::thread::id current, std::thread::id owner) const {
        // In debug builds, this will assert and provide detailed info
        // In release builds, we still want to fail fast rather than corrupt state

        char buffer[256];
        snprintf(buffer, sizeof(buffer),
                 "Thread ownership violation in %s!\n"
                 "Owner thread ID: %llu\n"
                 "Current thread ID: %llu\n"
                 "This component is not thread-safe and must only be accessed from its owner thread.",
                 m_ComponentName,
                 static_cast<unsigned long long>(std::hash<std::thread::id>{}(owner)),
                 static_cast<unsigned long long>(std::hash<std::thread::id>{}(current)));

#ifdef _DEBUG
        // In debug, assert with message
        assert(false && buffer);
#else
        // In release, log and terminate to prevent corruption
        fprintf(stderr, "%s\n", buffer);
        std::terminate();
#endif
    }

    const char *m_ComponentName;
    mutable std::atomic<std::thread::id> m_OwnerThread;
    mutable std::atomic<bool> m_IsInitialized;
};

/**
 * RAII helper for temporarily relaxing thread checks (e.g., during controlled ownership transfer)
 * Use with extreme caution!
 */
class ThreadOwnershipTransfer {
public:
    explicit ThreadOwnershipTransfer(ThreadOwnershipValidator &validator) : m_Validator(validator) {
        m_Validator.ResetOwnership();
    }

    ~ThreadOwnershipTransfer() {
        // New owner will claim on first access
    }

private:
    ThreadOwnershipValidator &m_Validator;
};

/**
 * @file ServiceContainer.h
 * @brief Lightweight IoC (Inversion of Control) Container
 *
 * This file implements a simple but functional dependency injection container
 * for managing TASEngine subsystems and their dependencies.
 *
 * Features:
 * - Service registration (Singleton/Transient)
 * - Dependency resolution
 * - Factory-based creation
 * - Type-safe service retrieval
 *
 * Architecture Pattern: Service Locator + Dependency Injection
 */

#pragma once

#include "Result.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <typeindex>

// ============================================================================
// Service Lifetime Enum
// ============================================================================
enum class ServiceLifetime {
    Singleton, // Single instance shared across all requests
    Transient  // New instance created for each request
};

// ============================================================================
// Service Descriptor (Internal)
// ============================================================================
class ServiceDescriptor {
public:
    virtual ~ServiceDescriptor() = default;
    virtual void *Resolve() = 0;
    virtual bool IsSingleton() const = 0;
};

// ============================================================================
// Typed Service Descriptor
// ============================================================================
template <typename T>
class TypedServiceDescriptor : public ServiceDescriptor {
public:
    // Use shared_ptr in factory signature to avoid deleter type issues
    using Factory = std::function<std::shared_ptr<T>()>;

    TypedServiceDescriptor(Factory factory, ServiceLifetime lifetime)
        : m_Factory(std::move(factory)), m_Lifetime(lifetime) {}

    void *Resolve() override {
        if (m_Lifetime == ServiceLifetime::Singleton) {
            if (!m_Instance) {
                m_Instance = m_Factory();
            }
            return m_Instance.get();
        } else {
            // For transient, create a new instance each time
            auto instance = m_Factory();
            return instance.get(); // Return raw pointer, caller manages lifetime via shared_ptr
        }
    }

    bool IsSingleton() const override {
        return m_Lifetime == ServiceLifetime::Singleton;
    }

private:
    Factory m_Factory;
    ServiceLifetime m_Lifetime;
    std::shared_ptr<T> m_Instance; // Use shared_ptr to type-erase deleter
};

// ============================================================================
// ServiceContainer - IoC Container
// ============================================================================
/**
 * @class ServiceContainer
 * @brief Lightweight dependency injection container
 *
 * Usage Example:
 * @code
 * ServiceContainer container;
 *
 * // Register services
 * container.RegisterSingleton<InputSystem>([]{
 *     return std::make_unique<InputSystem>();
 * });
 *
 * // Resolve services
 * auto inputSystem = container.Resolve<InputSystem>();
 * @endcode
 */
class ServiceContainer {
public:
    ServiceContainer() = default;
    ~ServiceContainer() = default;

    // ServiceContainer is not copyable or movable (manages singleton lifetimes)
    ServiceContainer(const ServiceContainer &) = delete;
    ServiceContainer &operator=(const ServiceContainer &) = delete;

    // ========================================================================
    // Service Registration
    // ========================================================================

    /**
     * @brief Register a service as singleton
     * @tparam T Service type
     * @param factory Factory function to create the service (can return unique_ptr or shared_ptr)
     */
    template <typename T>
    void RegisterSingleton(std::function<std::shared_ptr<T>()> factory) {
        auto descriptor = std::make_unique<TypedServiceDescriptor<T>>(
            std::move(factory), ServiceLifetime::Singleton);
        m_Services[std::type_index(typeid(T))] = std::move(descriptor);
    }

    /**
     * @brief Register a service as singleton (unique_ptr overload for convenience)
     * @tparam T Service type
     * @param factory Factory function returning unique_ptr
     */
    template <typename T>
    void RegisterSingleton(std::function<std::unique_ptr<T>()> factory) {
        auto sharedFactory = [factory = std::move(factory)]() {
            return std::shared_ptr<T>(factory());
        };
        RegisterSingleton<T>(sharedFactory);
    }

    /**
     * @brief Register an existing singleton instance (takes ownership)
     * @tparam T Service type
     * @param instance Existing instance to register (ownership transferred to container)
     */
    template <typename T>
    void RegisterSingletonInstance(std::unique_ptr<T> instance) {
        auto capturedInstance = std::shared_ptr<T>(std::move(instance));
        RegisterSingleton<T>([capturedInstance]() {
            // Return the shared_ptr (container owns the object)
            return capturedInstance;
        });
    }

    /**
     * @brief Register a singleton pointer (without taking ownership)
     * @tparam T Service type
     * @param ptr Raw pointer to existing instance (caller retains ownership)
     *
     * Use this when you want to register an existing service in the container
     * without transferring ownership. The caller is responsible for the lifetime
     * of the object.
     */
    template <typename T>
    void RegisterSingletonPtr(T *ptr) {
        RegisterSingleton<T>([ptr]() {
            // Create a shared_ptr that doesn't own the object (no-op deleter)
            return std::shared_ptr<T>(ptr, [](T *) {
            });
        });
    }

    /**
     * @brief Register a service as transient
     * @tparam T Service type
     * @param factory Factory function to create the service
     */
    template <typename T>
    void RegisterTransient(std::function<std::unique_ptr<T>()> factory) {
        auto descriptor = std::make_unique<TypedServiceDescriptor<T>>(
            std::move(factory), ServiceLifetime::Transient);
        m_Services[std::type_index(typeid(T))] = std::move(descriptor);
    }

    /**
     * @brief Register a service with explicit lifetime
     * @tparam T Service type
     * @param factory Factory function
     * @param lifetime Service lifetime
     */
    template <typename T>
    void Register(std::function<std::unique_ptr<T>()> factory, ServiceLifetime lifetime) {
        if (lifetime == ServiceLifetime::Singleton) {
            RegisterSingleton<T>(std::move(factory));
        } else {
            RegisterTransient<T>(std::move(factory));
        }
    }

    // ========================================================================
    // Service Resolution
    // ========================================================================

    /**
     * @brief Resolve a service
     * @tparam T Service type
     * @return Pointer to the service (nullptr if not registered)
     */
    template <typename T>
    T *Resolve() {
        auto it = m_Services.find(std::type_index(typeid(T)));
        if (it == m_Services.end()) {
            return nullptr;
        }

        return static_cast<T *>(it->second->Resolve());
    }

    /**
     * @brief Resolve a service with Result<T> error handling
     * @tparam T Service type
     * @return Result containing pointer to service or error
     */
    template <typename T>
    Result<T *> TryResolve() {
        auto it = m_Services.find(std::type_index(typeid(T)));
        if (it == m_Services.end()) {
            return Result<T *>::Error(
                std::string("Service not registered: ") + typeid(T).name(),
                "service_not_found"
            );
        }

        T *service = static_cast<T *>(it->second->Resolve());
        if (!service) {
            return Result<T *>::Error(
                std::string("Failed to resolve service: ") + typeid(T).name(),
                "resolution_failed"
            );
        }

        return Result<T *>::Ok(service);
    }

    /**
     * @brief Check if a service is registered
     * @tparam T Service type
     * @return True if registered
     */
    template <typename T>
    bool IsRegistered() const {
        return m_Services.find(std::type_index(typeid(T))) != m_Services.end();
    }

    /**
     * @brief Check if a service is singleton
     * @tparam T Service type
     * @return True if singleton, false if transient or not registered
     */
    template <typename T>
    bool IsSingleton() const {
        auto it = m_Services.find(std::type_index(typeid(T)));
        if (it == m_Services.end()) {
            return false;
        }
        return it->second->IsSingleton();
    }

    // ========================================================================
    // Container Management
    // ========================================================================

    /**
     * @brief Clear all registered services
     */
    void Clear() {
        m_Services.clear();
    }

    /**
     * @brief Get number of registered services
     */
    size_t GetServiceCount() const {
        return m_Services.size();
    }

private:
    std::unordered_map<std::type_index, std::unique_ptr<ServiceDescriptor>> m_Services;
};

// ============================================================================
// ServiceProvider - Read-only service access
// ============================================================================
/**
 * @class ServiceProvider
 * @brief Read-only interface for service resolution
 *
 * This is a lightweight wrapper that only allows service resolution,
 * not registration. Useful for passing to subsystems that should only
 * consume services, not register them.
 */
class ServiceProvider {
public:
    explicit ServiceProvider(ServiceContainer &container)
        : m_Container(&container) {
    }

    template <typename T>
    T *Resolve() {
        return m_Container->Resolve<T>();
    }

    template <typename T>
    Result<T *> TryResolve() {
        return m_Container->TryResolve<T>();
    }

    template <typename T>
    bool IsRegistered() const {
        return m_Container->IsRegistered<T>();
    }

private:
    ServiceContainer *m_Container;
};

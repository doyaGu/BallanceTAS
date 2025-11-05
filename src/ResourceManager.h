#pragma once

#include "Result.h"
#include <memory>
#include <vector>
#include <functional>
#include <filesystem>
#include <string>
#include <mutex>

// ============================================================================
// Resource Manager - RAII Resource Management
// ============================================================================
class ResourceManager {
public:
    ResourceManager() = default;
    ~ResourceManager();

    // Disable copy, allow move
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager &operator=(const ResourceManager &) = delete;
    ResourceManager(ResourceManager &&) noexcept = default;
    ResourceManager &operator=(ResourceManager &&) noexcept = default;

    // ========================================================================
    // Temporary File Management
    // ========================================================================
    class TemporaryFile {
    public:
        TemporaryFile(std::filesystem::path path, bool autoDelete = true);
        ~TemporaryFile();

        // Disable copy, allow move
        TemporaryFile(const TemporaryFile &) = delete;
        TemporaryFile &operator=(const TemporaryFile &) = delete;
        TemporaryFile(TemporaryFile &&other) noexcept;
        TemporaryFile &operator=(TemporaryFile &&other) noexcept;

        const std::filesystem::path &GetPath() const { return m_Path; }
        std::string GetPathString() const { return m_Path.string(); }
        bool Exists() const;

        // Cancel auto delete
        void KeepFile() { m_AutoDelete = false; }

        // Delete immediately
        Result<void> Delete();

    private:
        std::filesystem::path m_Path;
        bool m_AutoDelete;
        bool m_Deleted = false;
    };

    // Create temporary file
    std::shared_ptr<TemporaryFile> CreateTempFile(
        const std::string &prefix = "tas_",
        const std::string &extension = ".tmp"
    );

    // Create temporary directory
    std::shared_ptr<TemporaryFile> CreateTempDirectory(
        const std::string &prefix = "tas_dir_"
    );

    // ========================================================================
    // Cleanup Callback Management
    // ========================================================================
    using CleanupHandler = std::function<void()>;

    // Register cleanup callback (executed in reverse order of registration)
    void RegisterCleanupHandler(CleanupHandler handler);

    // Register named cleanup callback (can be cancelled)
    void RegisterCleanupHandler(const std::string &name, CleanupHandler handler);

    // Unregister named cleanup callback
    bool UnregisterCleanupHandler(const std::string &name);

    // Execute all cleanup immediately
    void CleanupAll();

    // Get temporary file count
    size_t GetTempFileCount() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_TempFiles.size();
    }

    // Get cleanup handler count
    size_t GetCleanupHandlerCount() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_CleanupHandlers.size() + m_NamedCleanupHandlers.size();
    }

    // ========================================================================
    // RAII Helper Class - Automatic cleanup at scope end
    // ========================================================================
    class ScopedResource {
    public:
        explicit ScopedResource(ResourceManager *manager, CleanupHandler cleanup)
            : m_Manager(manager), m_Cleanup(std::move(cleanup)), m_Active(true) {
        }

        ~ScopedResource() {
            if (m_Active && m_Cleanup) {
                try {
                    m_Cleanup();
                } catch (...) {
                    // Destructors should not throw exceptions
                }
            }
        }

        // Disable copy, allow move
        ScopedResource(const ScopedResource &) = delete;
        ScopedResource &operator=(const ScopedResource &) = delete;

        ScopedResource(ScopedResource &&other) noexcept
            : m_Manager(other.m_Manager), m_Cleanup(std::move(other.m_Cleanup)), m_Active(other.m_Active) {
            other.m_Active = false;
        }

        ScopedResource &operator=(ScopedResource &&other) noexcept {
            if (this != &other) {
                if (m_Active && m_Cleanup) {
                    m_Cleanup();
                }
                m_Manager = other.m_Manager;
                m_Cleanup = std::move(other.m_Cleanup);
                m_Active = other.m_Active;
                other.m_Active = false;
            }
            return *this;
        }

        // Cancel cleanup
        void Release() { m_Active = false; }

        // Execute cleanup immediately
        void Cleanup() {
            if (m_Active && m_Cleanup) {
                m_Cleanup();
                m_Active = false;
            }
        }

    private:
        ResourceManager *m_Manager;
        CleanupHandler m_Cleanup;
        bool m_Active;
    };

    // Create scoped resource
    template <typename F>
    ScopedResource CreateScopedResource(F &&cleanup) {
        return ScopedResource(this, std::forward<F>(cleanup));
    }

private:
    mutable std::mutex m_Mutex;
    std::vector<std::shared_ptr<TemporaryFile>> m_TempFiles;
    std::vector<CleanupHandler> m_CleanupHandlers;
    std::unordered_map<std::string, CleanupHandler> m_NamedCleanupHandlers;

    bool m_IsCleanedUp = false;

    // Get temporary directory path
    static std::filesystem::path GetTempDirectory();

    // Generate unique filename
    static std::string GenerateUniqueFilename(const std::string &prefix, const std::string &extension);
};

// ============================================================================
// Global Resource Manager (Optional)
// ============================================================================
class GlobalResourceManager {
public:
    static ResourceManager &Instance() {
        static ResourceManager instance;
        return instance;
    }

private:
    GlobalResourceManager() = default;
};

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

    // 禁用拷贝，允许移动
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = default;
    ResourceManager& operator=(ResourceManager&&) noexcept = default;

    // ========================================================================
    // 临时文件管理
    // ========================================================================
    class TemporaryFile {
    public:
        TemporaryFile(std::filesystem::path path, bool autoDelete = true);
        ~TemporaryFile();

        // 禁用拷贝，允许移动
        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;
        TemporaryFile(TemporaryFile&& other) noexcept;
        TemporaryFile& operator=(TemporaryFile&& other) noexcept;

        const std::filesystem::path& GetPath() const { return m_Path; }
        std::string GetPathString() const { return m_Path.string(); }
        bool Exists() const;

        // 取消自动删除
        void KeepFile() { m_AutoDelete = false; }

        // 立即删除
        Result<void> Delete();

    private:
        std::filesystem::path m_Path;
        bool m_AutoDelete;
        bool m_Deleted = false;
    };

    // 创建临时文件
    std::shared_ptr<TemporaryFile> CreateTempFile(
        const std::string& prefix = "tas_",
        const std::string& extension = ".tmp"
    );

    // 创建临时目录
    std::shared_ptr<TemporaryFile> CreateTempDirectory(
        const std::string& prefix = "tas_dir_"
    );

    // ========================================================================
    // 清理回调管理
    // ========================================================================
    using CleanupHandler = std::function<void()>;

    // 注册清理回调（按注册顺序的逆序执行）
    void RegisterCleanupHandler(CleanupHandler handler);

    // 注册命名的清理回调（可以取消）
    void RegisterCleanupHandler(const std::string& name, CleanupHandler handler);

    // 取消命名的清理回调
    bool UnregisterCleanupHandler(const std::string& name);

    // 立即执行所有清理
    void CleanupAll();

    // 获取临时文件计数
    size_t GetTempFileCount() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_TempFiles.size();
    }

    // 获取清理回调计数
    size_t GetCleanupHandlerCount() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_CleanupHandlers.size() + m_NamedCleanupHandlers.size();
    }

    // ========================================================================
    // RAII辅助类 - 作用域结束时自动清理
    // ========================================================================
    class ScopedResource {
    public:
        explicit ScopedResource(ResourceManager* manager, CleanupHandler cleanup)
            : m_Manager(manager)
            , m_Cleanup(std::move(cleanup))
            , m_Active(true)
        {}

        ~ScopedResource() {
            if (m_Active && m_Cleanup) {
                try {
                    m_Cleanup();
                } catch (...) {
                    // 析构函数不应抛出异常
                }
            }
        }

        // 禁用拷贝，允许移动
        ScopedResource(const ScopedResource&) = delete;
        ScopedResource& operator=(const ScopedResource&) = delete;

        ScopedResource(ScopedResource&& other) noexcept
            : m_Manager(other.m_Manager)
            , m_Cleanup(std::move(other.m_Cleanup))
            , m_Active(other.m_Active)
        {
            other.m_Active = false;
        }

        ScopedResource& operator=(ScopedResource&& other) noexcept {
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

        // 取消清理
        void Release() { m_Active = false; }

        // 立即执行清理
        void Cleanup() {
            if (m_Active && m_Cleanup) {
                m_Cleanup();
                m_Active = false;
            }
        }

    private:
        ResourceManager* m_Manager;
        CleanupHandler m_Cleanup;
        bool m_Active;
    };

    // 创建作用域资源
    template <typename F>
    ScopedResource CreateScopedResource(F&& cleanup) {
        return ScopedResource(this, std::forward<F>(cleanup));
    }

private:
    mutable std::mutex m_Mutex;
    std::vector<std::shared_ptr<TemporaryFile>> m_TempFiles;
    std::vector<CleanupHandler> m_CleanupHandlers;
    std::unordered_map<std::string, CleanupHandler> m_NamedCleanupHandlers;

    bool m_IsCleanedUp = false;

    // 获取临时目录路径
    static std::filesystem::path GetTempDirectory();

    // 生成唯一文件名
    static std::string GenerateUniqueFilename(const std::string& prefix, const std::string& extension);
};

// ============================================================================
// 全局资源管理器（可选）
// ============================================================================
class GlobalResourceManager {
public:
    static ResourceManager& Instance() {
        static ResourceManager instance;
        return instance;
    }

private:
    GlobalResourceManager() = default;
};

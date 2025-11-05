#include "ResourceManager.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <fstream>

// ============================================================================
// TemporaryFile Implementation
// ============================================================================

ResourceManager::TemporaryFile::TemporaryFile(std::filesystem::path path, bool autoDelete)
    : m_Path(std::move(path)), m_AutoDelete(autoDelete) {
}

ResourceManager::TemporaryFile::~TemporaryFile() {
    if (m_AutoDelete && !m_Deleted) {
        try {
            Delete();
        } catch (...) {
            // Destructors should not throw exceptions
        }
    }
}

ResourceManager::TemporaryFile::TemporaryFile(TemporaryFile &&other) noexcept
    : m_Path(std::move(other.m_Path)), m_AutoDelete(other.m_AutoDelete), m_Deleted(other.m_Deleted) {
    other.m_Deleted = true; // Prevent deletion when other destructs
}

ResourceManager::TemporaryFile &ResourceManager::TemporaryFile::operator=(TemporaryFile &&other) noexcept {
    if (this != &other) {
        if (m_AutoDelete && !m_Deleted) {
            try {
                Delete();
            } catch (...) {
            }
        }

        m_Path = std::move(other.m_Path);
        m_AutoDelete = other.m_AutoDelete;
        m_Deleted = other.m_Deleted;
        other.m_Deleted = true;
    }
    return *this;
}

bool ResourceManager::TemporaryFile::Exists() const {
    return std::filesystem::exists(m_Path);
}

Result<void> ResourceManager::TemporaryFile::Delete() {
    if (m_Deleted) {
        return Result<void>::Ok();
    }

    try {
        if (std::filesystem::exists(m_Path)) {
            if (std::filesystem::is_directory(m_Path)) {
                std::filesystem::remove_all(m_Path);
            } else {
                std::filesystem::remove(m_Path);
            }
        }
        m_Deleted = true;
        return Result<void>::Ok();
    } catch (const std::filesystem::filesystem_error &e) {
        return Result<void>::Error(
            std::string("Failed to delete file: ") + e.what(),
            "filesystem",
            ErrorSeverity::Warning
        );
    }
}

// ============================================================================
// ResourceManager Implementation
// ============================================================================

ResourceManager::~ResourceManager() {
    CleanupAll();
}

std::shared_ptr<ResourceManager::TemporaryFile> ResourceManager::CreateTempFile(
    const std::string &prefix,
    const std::string &extension) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto tempDir = GetTempDirectory();
    auto filename = GenerateUniqueFilename(prefix, extension);
    auto fullPath = tempDir / filename;

    // Create empty file
    try {
        std::ofstream file(fullPath);
        if (!file) {
            return nullptr;
        }
    } catch (...) {
        return nullptr;
    }

    auto tempFile = std::make_shared<TemporaryFile>(fullPath, true);
    m_TempFiles.push_back(tempFile);

    return tempFile;
}

std::shared_ptr<ResourceManager::TemporaryFile> ResourceManager::CreateTempDirectory(
    const std::string &prefix) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    auto tempDir = GetTempDirectory();
    auto dirname = GenerateUniqueFilename(prefix, "");
    auto fullPath = tempDir / dirname;

    // Create directory
    try {
        std::filesystem::create_directories(fullPath);
    } catch (...) {
        return nullptr;
    }

    auto tempFile = std::make_shared<TemporaryFile>(fullPath, true);
    m_TempFiles.push_back(tempFile);

    return tempFile;
}

void ResourceManager::RegisterCleanupHandler(CleanupHandler handler) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CleanupHandlers.push_back(std::move(handler));
}

void ResourceManager::RegisterCleanupHandler(const std::string &name, CleanupHandler handler) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_NamedCleanupHandlers[name] = std::move(handler);
}

bool ResourceManager::UnregisterCleanupHandler(const std::string &name) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_NamedCleanupHandlers.erase(name) > 0;
}

void ResourceManager::CleanupAll() {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (m_IsCleanedUp) {
        return;
    }

    m_IsCleanedUp = true;

    // 1. Execute cleanup callbacks (in reverse order)
    for (auto it = m_CleanupHandlers.rbegin(); it != m_CleanupHandlers.rend(); ++it) {
        try {
            (*it)();
        } catch (...) {
            // Ignore exceptions, continue cleanup
        }
    }

    // 2. Execute named cleanup callbacks
    for (auto &[name, handler] : m_NamedCleanupHandlers) {
        try {
            handler();
        } catch (...) {
            // Ignore exceptions, continue cleanup
        }
    }

    // 3. Clean up temporary files (in reverse order)
    for (auto it = m_TempFiles.rbegin(); it != m_TempFiles.rend(); ++it) {
        try {
            (*it)->Delete();
        } catch (...) {
            // Ignore exceptions, continue cleanup
        }
    }

    m_CleanupHandlers.clear();
    m_NamedCleanupHandlers.clear();
    m_TempFiles.clear();
}

std::filesystem::path ResourceManager::GetTempDirectory() {
    auto tempPath = std::filesystem::temp_directory_path() / "BallanceTAS";

    // Ensure directory exists
    try {
        std::filesystem::create_directories(tempPath);
    } catch (...) {
        // If creation fails, use system temp directory
        return std::filesystem::temp_directory_path();
    }

    return tempPath;
}

std::string ResourceManager::GenerateUniqueFilename(const std::string &prefix, const std::string &extension) {
    // Generate unique filename using current timestamp and random number
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    std::ostringstream oss;
    oss << prefix << timestamp << "_" << dis(gen) << extension;

    return oss.str();
}

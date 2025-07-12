#pragma once

#include <string>
#include <sol/sol.hpp>

/**
 * @enum ProjectType
 * @brief Different types of TAS projects supported by the system.
 */
enum class ProjectType {
    Script, // Lua script-based projects (current system)
    Record, // Binary .tas record files (legacy system)
    Mixed   // Projects with both script and record (future)
};

/**
 * @class TASProject
 * @brief Represents a single TAS project found on the filesystem.
 *
 * This class now supports both script-based projects (manifest.lua + main.lua)
 * and record-based projects (single .tas file). It stores metadata and provides
 * convenient accessors for different project types.
 *
 * Supports both directory-based and zip-based projects.
 */
class TASProject {
public:
    // Constructor for script-based projects
    explicit TASProject(std::string projectPath, sol::table manifest);

    // Constructor for record-based projects (.tas files)
    explicit TASProject(std::string tasFilePath);

    // --- Type Information ---
    ProjectType GetProjectType() const { return m_ProjectType; }
    bool IsScriptProject() const { return m_ProjectType == ProjectType::Script; }
    bool IsRecordProject() const { return m_ProjectType == ProjectType::Record; }

    // --- Accessors for Manifest Data ---
    sol::table GetManifestTable() const { return m_Manifest; }

    const std::string &GetName() const { return m_Name; }
    const std::string &GetAuthor() const { return m_Author; }
    const std::string &GetDescription() const { return m_Description; }
    const std::string &GetTargetLevel() const { return m_TargetLevel; }
    const std::string &GetEntryScript() const { return m_EntryScript; }
    float GetUpdateRate() const { return m_UpdateRate; }
    float GetDeltaTime() const { return m_DeltaTime; }

    // --- Legacy Mode Settings ---
    bool RequiresLegacyMode() const { return m_LegacyMode; }

    // --- Path Accessors ---
    const std::string &GetPath() const { return m_ProjectPath; }

    /**
     * @brief Gets the path to the TAS record file (for record projects).
     * @return Path to the .tas file, or empty string for script projects.
     */
    std::string GetRecordFilePath() const;

    /**
     * @brief Gets the path to the entry script for execution (for script projects).
     * @param executionBasePath The base path to use for execution (for zip projects).
     * @return Full path to the entry script, or empty string for record projects.
     */
    std::string GetEntryScriptPath(const std::string &executionBasePath = "") const;

    /**
     * @brief Gets a file path within the project for execution.
     * @param fileName The relative file name within the project.
     * @param executionBasePath The base path to use for execution.
     * @return Full path to the file.
     */
    std::string GetProjectFilePath(const std::string &fileName, const std::string &executionBasePath = "") const;

    bool IsValid() const { return m_IsValid; }

    bool IsZipProject() const { return m_IsZipProject; }
    void SetIsZipProject(bool isZip) { m_IsZipProject = isZip; }

    /**
     * @brief Sets the execution base path (used for extracted zip projects).
     * @param basePath The path where the project has been extracted for execution.
     */
    void SetExecutionBasePath(const std::string &basePath) { m_ExecutionBasePath = basePath; }

    /**
     * @brief Gets the execution base path.
     * @return The execution base path, or the original project path if not set.
     */
    std::string GetExecutionBasePath() const {
        return m_ExecutionBasePath.empty() ? m_ProjectPath : m_ExecutionBasePath;
    }

    /**
     * @brief Checks if the project is ready for execution.
     * @return True if the project is ready for execution.
     */
    bool IsReadyForExecution() const {
        if (IsRecordProject()) {
            // Record projects just need the .tas file to exist
            return m_IsValid;
        }
        // Script projects need extraction if they're zip-based
        return !m_IsZipProject || !m_ExecutionBasePath.empty();
    }

    /**
     * @brief Checks if the project can be played with current BML settings.
     * @param currentLegacyMode Current BML legacy mode setting.
     * @return True if the project is compatible with current settings.
     */
    bool IsCompatibleWithSettings(bool currentLegacyMode) const;

    /**
     * @brief Gets a human-readable compatibility message.
     * @param currentLegacyMode Current BML legacy mode setting.
     * @return Compatibility message, or empty string if compatible.
     */
    std::string GetCompatibilityMessage(bool currentLegacyMode) const;

    /**
     * @brief Gets all requirement strings for UI display.
     * @return Vector of requirement strings.
     */
    std::vector<std::string> GetRequirements() const;

private:
    void ParseManifest(const sol::table &manifest);
    void ParseRecordProject(const std::string &tasFilePath);

    std::string m_ProjectPath;       // Original path (zip file path for zip projects, .tas file for record projects)
    std::string m_ExecutionBasePath; // Path for execution (temp directory for zip projects)
    sol::table m_Manifest;           // Keep a copy of the raw manifest table (invalid for record projects)

    ProjectType m_ProjectType = ProjectType::Script;

    // Parsed and cached data
    std::string m_Name = "Unnamed TAS";
    std::string m_Author = "Unknown";
    std::string m_Description = "No description.";
    std::string m_EntryScript = "main.lua";
    std::string m_TargetLevel;
    float m_UpdateRate = 132.0f; // Default to 132 = 66 * 2 (game's physics update rate)
    float m_DeltaTime = 1000.0f / 132.0f;
    bool m_LegacyMode = false;

    bool m_IsValid = false;
    bool m_IsZipProject = false;
};

#pragma once

#include <string>
#include <sol/sol.hpp>

/**
 * @class TASProject
 * @brief Represents a single TAS project found on the filesystem.
 *
 * This class stores the metadata parsed from a project's manifest.lua file,
 * such as its name, author, and target level. It also provides convenient
 * accessors for file paths within the project directory.
 *
 * Supports both directory-based and zip-based projects.
 */
class TASProject {
public:
    // A project is defined by its root directory path or zip file path.
    explicit TASProject(std::string projectPath, sol::table manifest);

    // --- Accessors for Manifest Data ---
    const std::string &GetName() const { return m_Name; }
    const std::string &GetAuthor() const { return m_Author; }
    const std::string &GetDescription() const { return m_Description; }
    const std::string &GetTargetLevel() const { return m_TargetLevel; }
    const std::string &GetEntryScript() const { return m_EntryScript; }
    float GetUpdateRate() const { return m_UpdateRate; }
    float GetDeltaTime() const { return m_DeltaTime; }
    sol::table GetManifestTable() const { return m_Manifest; }

    // --- Path Accessors ---
    const std::string &GetPath() const { return m_ProjectPath; }

    /**
     * @brief Gets the path to the entry script for execution.
     * For zip projects, this should be called after the project has been
     * prepared for execution (extracted to temp directory).
     * @param executionBasePath The base path to use for execution (for zip projects, this is the temp directory).
     * @return Full path to the entry script.
     */
    std::string GetEntryScriptPath(const std::string &executionBasePath = "") const;

    /**
     * @brief Gets a file path within the project for execution.
     * @param fileName The relative file name within the project.
     * @param executionBasePath The base path to use for execution (for zip projects, this is the temp directory).
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
     * For directory projects, always returns true.
     * For zip projects, returns true only if execution base path is set.
     * @return True if the project is ready for execution.
     */
    bool IsReadyForExecution() const {
        return !m_IsZipProject || !m_ExecutionBasePath.empty();
    }

private:
    void ParseManifest(const sol::table &manifest);

    std::string m_ProjectPath;       // Original path (zip file path for zip projects)
    std::string m_ExecutionBasePath; // Path for execution (temp directory for zip projects)
    sol::table m_Manifest;           // Keep a copy of the raw manifest table

    // Parsed and cached data
    std::string m_Name = "Unnamed TAS";
    std::string m_Author = "Unknown";
    std::string m_Description = "No description.";
    std::string m_EntryScript = "main.lua";
    std::string m_TargetLevel;
    float m_UpdateRate = 132.0f; // Default to 132 = 66 * 2 (game's physics update rate)
    float m_DeltaTime = 1 / 132.0f * 1000;

    bool m_IsValid = false;
    bool m_IsZipProject = false;
};

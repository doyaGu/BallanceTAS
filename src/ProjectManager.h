#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <sol/sol.hpp>
#include "TASProject.h"

class BallanceTAS;

/**
 * @class ProjectManager
 * @brief Discovers, loads, and manages TAS projects from the filesystem.
 * Supports both directory-based and zip-based projects with automatic extraction.
 */
class ProjectManager {
public:
    ProjectManager(BallanceTAS *mod, sol::state &lua_state);
    ~ProjectManager();

    /**
     * @brief Scans the TAS directory and reloads the list of all available projects.
     */
    void RefreshProjects();

    /**
     * @brief Extracts a zip file to a temporary directory for execution.
     * @param zipPath Path to the zip file to extract.
     * @param tempDir Directory to extract to (will be created).
     * @return True if extraction was successful.
     */
    bool ExtractZipProject(const std::string &zipPath, const std::string &tempDir);

    /**
     * @brief Prepares a project for execution by extracting it if it's a zip file.
     * @param project The project to prepare.
     * @return The path to the executable project (may be temp directory for zip projects).
     */
    std::string PrepareProjectForExecution(TASProject *project);

    /**
     * @brief Creates a zip archive from an existing project directory.
     * @param projectPath Path to the project directory.
     * @param zipPath Output path for the zip file.
     * @return True if compression was successful.
     */
    bool CompressProject(const std::string &projectPath, const std::string &zipPath);

    /**
     * @brief Validates that a zip file contains a valid TAS project structure.
     * @param zipPath Path to the zip file to validate.
     * @return True if the zip contains a valid project.
     */
    bool ValidateZipProject(const std::string &zipPath);

    // --- Accessors ---
    const std::vector<std::unique_ptr<TASProject>> &GetProjects() const { return m_Projects; }
    TASProject *GetCurrentProject() const { return m_CurrentProject; }
    void SetCurrentProject(TASProject *project);

    /**
     * @brief Gets the temporary directory used for extracted zip projects.
     * @return Path to the temporary directory.
     */
    std::string GetTempDirectory() const { return m_TempDir; }

    /**
     * @brief Cleans up temporary directories used for zip extraction.
     */
    void CleanupTempDirectories();

    /**
     * @brief Cleans up temporary directory for a specific project.
     * @param project The project whose temp directory should be cleaned up.
     */
    void CleanupProjectTempDirectory(TASProject *project);

private:
    /**
     * @brief Loads a single project from a directory.
     * @param projectPath The root directory of the project to load.
     * @return A unique_ptr to the TASProject, or nullptr if loading fails.
     */
    std::unique_ptr<TASProject> LoadDirectoryProject(const std::string &projectPath);

    /**
     * @brief Loads a single project from a zip file.
     * @param zipPath Path to the zip file containing the project.
     * @return A unique_ptr to the TASProject, or nullptr if loading fails.
     */
    std::unique_ptr<TASProject> LoadZipProject(const std::string &zipPath);

    /**
     * @brief Parses a manifest.lua file in a secure, sandboxed Lua environment.
     * @param path The full path to the manifest.lua file.
     * @return A sol::table containing the manifest data, or an invalid table on error.
     */
    sol::table ParseManifestFile(const std::string &path);

    /**
     * @brief Reads a file from within a zip archive.
     * @param zipPath Path to the zip file.
     * @param fileName Name of the file within the zip.
     * @param content Output string for the file content.
     * @return True if the file was read successfully.
     */
    bool ReadFileFromZip(const std::string &zipPath, const std::string &fileName, std::string &content);

    /**
     * @brief Creates a unique temporary directory for zip extraction.
     * @param baseName Base name for the temporary directory.
     * @return Path to the created temporary directory.
     */
    std::string CreateTempDirectory(const std::string &baseName);

    /**
     * @brief Recursively removes a directory and its contents.
     * @param dirPath Path to the directory to remove.
     * @return True if removal was successful.
     */
    bool RemoveDirectory(const std::string &dirPath);

    /**
     * @brief Gets the project name from a zip file by reading its manifest.
     * @param zipPath Path to the zip file.
     * @return The project name, or the filename if manifest can't be read.
     */
    std::string GetProjectNameFromZip(const std::string &zipPath);

    /**
     * @brief Normalizes a file path by converting separators to the platform standard.
     * @param path The path to normalize.
     * @return The normalized path.
     */
    std::string NormalizePath(const std::string &path);

    /**
     * @brief Validates that all required files exist in a directory.
     * @param projectPath Path to the project directory.
     * @return True if all required files are present.
     */
    bool ValidateProjectStructure(const std::string &projectPath);

    BallanceTAS *m_Mod;
    sol::state &m_LuaState; // Reference to the main Lua state for creating sandboxes.
    std::string m_TASRootPath;
    std::string m_TempDir; // Base directory for temporary extractions

    std::vector<std::unique_ptr<TASProject>> m_Projects;
    TASProject *m_CurrentProject = nullptr; // Current project being worked on, if any.

    // Track temporary directories for cleanup - maps project pointer to temp directory path
    std::unordered_map<TASProject *, std::string> m_ProjectTempDirectories;
    std::vector<std::string> m_TempDirectories; // Legacy cleanup list
};

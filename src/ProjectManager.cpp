#include "ProjectManager.h"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>

#include <zip.h>

#include "TASEngine.h"

namespace fs = std::filesystem;

ProjectManager::ProjectManager(TASEngine *engine)
    : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("ProjectManager requires a valid TASEngine instance.");
    }
    
    // Define the root directory for all TAS projects.
    m_TASRootPath = m_Engine->GetPath();

    // Create temp directory for zip extractions
    m_TempDir = m_TASRootPath + "temp\\";

    // Ensure the directories exist.
    if (!fs::exists(m_TASRootPath)) {
        fs::create_directories(m_TASRootPath);
    }
    if (!fs::exists(m_TempDir)) {
        fs::create_directories(m_TempDir);
    }

    RefreshProjects();
}

ProjectManager::~ProjectManager() {
    CleanupTempDirectories();
}

void ProjectManager::RefreshProjects() {
    // Clean up previous project temp directories
    CleanupTempDirectories();

    m_Projects.clear();
    m_ProjectTempDirectories.clear();
    m_CurrentProject = nullptr;

    m_Engine->GetLogger()->Info("Scanning for TAS projects in: %s", m_TASRootPath.c_str());

    int directoryProjects = 0;
    int zipProjects = 0;
    int recordProjects = 0;

    try {
        for (const auto &entry : fs::directory_iterator(m_TASRootPath)) {
            if (entry.is_directory()) {
                // Traditional directory-based script project
                std::string projectPath = NormalizePath(entry.path().string());
                std::string manifestPath = projectPath + "\\manifest.lua";
                if (fs::exists(manifestPath) && ValidateProjectStructure(projectPath)) {
                    auto project = LoadDirectoryProject(projectPath);
                    if (project && project->IsValid()) {
                        m_Projects.push_back(std::move(project));
                        directoryProjects++;
                    }
                }
            } else if (entry.is_regular_file()) {
                std::string filePath = NormalizePath(entry.path().string());
                std::string extension = entry.path().extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), tolower);

                if (extension == ".zip") {
                    // Zip-based script project
                    if (ValidateZipProject(filePath)) {
                        auto project = LoadZipProject(filePath);
                        if (project && project->IsValid()) {
                            m_Projects.push_back(std::move(project));
                            zipProjects++;
                        }
                    } else {
                        m_Engine->GetLogger()->Warn("Invalid zip project structure: %s", filePath.c_str());
                    }
                } else if (extension == ".tas") {
                    // Binary record project
                    auto project = LoadRecordProject(filePath);
                    if (project && project->IsValid()) {
                        m_Projects.push_back(std::move(project));
                        recordProjects++;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error &e) {
        m_Engine->GetLogger()->Error("Filesystem error while scanning for projects: %s", e.what());
    }

    // Sort projects alphabetically by name for consistent UI display.
    std::sort(m_Projects.begin(), m_Projects.end(), [](const auto &a, const auto &b) {
        return a->GetName() < b->GetName();
    });

    m_Engine->GetLogger()->Info("Found %d valid TAS projects (%d directories, %d zip files, %d record files).",
                             static_cast<int>(m_Projects.size()), directoryProjects, zipProjects, recordProjects);
}

std::unique_ptr<TASProject> ProjectManager::LoadDirectoryProject(const std::string &projectPath) {
    std::string manifestPath = projectPath + "\\manifest.lua";
    sol::table manifest_table = ParseManifestFile(manifestPath);

    if (!manifest_table.valid()) {
        m_Engine->GetLogger()->Warn("Could not parse manifest for directory project at: %s", projectPath.c_str());
        return nullptr;
    }

    auto project = std::make_unique<TASProject>(projectPath, manifest_table);
    project->SetIsZipProject(false);

    return project;
}

std::unique_ptr<TASProject> ProjectManager::LoadZipProject(const std::string &zipPath) {
    // Read manifest from zip
    std::string manifestContent;
    if (!ReadFileFromZip(zipPath, "manifest.lua", manifestContent)) {
        m_Engine->GetLogger()->Warn("Could not read manifest from zip project: %s", zipPath.c_str());
        return nullptr;
    }

    // Create temporary file for manifest
    std::string tempManifest = m_TempDir + "temp_manifest_" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + ".lua";

    try {
        std::ofstream tempFile(tempManifest);
        if (!tempFile.is_open()) {
            m_Engine->GetLogger()->Error("Could not create temporary manifest file: %s", tempManifest.c_str());
            return nullptr;
        }
        tempFile << manifestContent;
        tempFile.close();

        // Parse the temporary manifest
        sol::table manifest_table = ParseManifestFile(tempManifest);

        // Clean up temporary file
        fs::remove(tempManifest);

        if (!manifest_table.valid()) {
            m_Engine->GetLogger()->Warn("Could not parse manifest for zip project: %s", zipPath.c_str());
            return nullptr;
        }

        auto project = std::make_unique<TASProject>(zipPath, manifest_table);
        project->SetIsZipProject(true);

        return project;
    } catch (const std::exception &e) {
        // Ensure cleanup even on exceptions
        if (fs::exists(tempManifest)) {
            fs::remove(tempManifest);
        }
        m_Engine->GetLogger()->Error("Exception loading zip project %s: %s", zipPath.c_str(), e.what());
        return nullptr;
    }
}

std::unique_ptr<TASProject> ProjectManager::LoadRecordProject(const std::string &recordPath) {
    m_Engine->GetLogger()->Info("Loading record project: %s", recordPath.c_str());

    try {
        auto project = std::make_unique<TASProject>(recordPath);

        if (project->IsValid()) {
            m_Engine->GetLogger()->Info("Record project loaded: %s (%zu frames)",
                                     project->GetName().c_str(),
                                     project->IsValid() ? 0 : 0); // We could parse frame count here if needed
            return project;
        } else {
            m_Engine->GetLogger()->Warn("Invalid record project: %s", recordPath.c_str());
            return nullptr;
        }
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception loading record project %s: %s", recordPath.c_str(), e.what());
        return nullptr;
    }
}

std::string ProjectManager::PrepareProjectForExecution(TASProject *project) {
    if (!project) {
        m_Engine->GetLogger()->Error("Cannot prepare null project for execution.");
        return "";
    }

    // Record projects don't need preparation - they're single files
    if (project->IsRecordProject()) {
        return project->GetRecordFilePath();
    }

    // For script projects, handle as before...
    // For directory projects, return the path directly
    if (!project->IsZipProject()) {
        return project->GetPath();
    }

    // For zip projects, check if we already have an extracted version
    auto it = m_ProjectTempDirectories.find(project);
    if (it != m_ProjectTempDirectories.end()) {
        // Check if the temp directory still exists and is valid
        if (fs::exists(it->second) && ValidateProjectStructure(it->second)) {
            m_Engine->GetLogger()->Info("Using existing extracted project: %s", it->second.c_str());
            return it->second;
        } else {
            // Clean up invalid temp directory
            m_ProjectTempDirectories.erase(it);
        }
    }

    // Extract the zip project to a temporary directory
    std::string projectName = project->GetName();
    // Sanitize project name for directory creation
    std::replace_if(projectName.begin(), projectName.end(),
                    [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');

    std::string tempDirPath = CreateTempDirectory("project_" + projectName);

    if (!ExtractZipProject(project->GetPath(), tempDirPath)) {
        m_Engine->GetLogger()->Error("Failed to extract zip project for execution: %s", project->GetPath().c_str());
        RemoveDirectory(tempDirPath);
        return "";
    }

    // Validate the extracted project structure
    if (!ValidateProjectStructure(tempDirPath)) {
        m_Engine->GetLogger()->Error("Extracted zip project has invalid structure: %s", tempDirPath.c_str());
        RemoveDirectory(tempDirPath);
        return "";
    }

    // Store the temp directory for this project
    m_ProjectTempDirectories[project] = tempDirPath;

    m_Engine->GetLogger()->Info("Successfully prepared zip project for execution: %s -> %s",
                             project->GetPath().c_str(), tempDirPath.c_str());

    return tempDirPath;
}

bool ProjectManager::ExtractZipProject(const std::string &zipPath, const std::string &tempDir) {
    m_Engine->GetLogger()->Info("Extracting zip project: %s to %s", zipPath.c_str(), tempDir.c_str());

    // Create temporary directory
    try {
        if (!fs::create_directories(tempDir)) {
            if (!fs::exists(tempDir)) {
                m_Engine->GetLogger()->Error("Failed to create temp directory: %s", tempDir.c_str());
                return false;
            }
        }
    } catch (const fs::filesystem_error &e) {
        m_Engine->GetLogger()->Error("Filesystem error creating temp directory %s: %s", tempDir.c_str(), e.what());
        return false;
    }

    // Extract zip file using the zip library
    struct zip_t *zip = zip_open(zipPath.c_str(), 0, 'r');
    if (!zip) {
        m_Engine->GetLogger()->Error("Failed to open zip file: %s", zipPath.c_str());
        RemoveDirectory(tempDir);
        return false;
    }

    bool success = true;
    ssize_t totalEntries = zip_entries_total(zip);

    if (totalEntries <= 0) {
        m_Engine->GetLogger()->Error("Zip file appears to be empty or corrupted: %s", zipPath.c_str());
        zip_close(zip);
        RemoveDirectory(tempDir);
        return false;
    }

    m_Engine->GetLogger()->Info("Extracting %d entries from zip file...", static_cast<int>(totalEntries));

    for (ssize_t i = 0; i < totalEntries; ++i) {
        if (zip_entry_openbyindex(zip, i) != 0) {
            m_Engine->GetLogger()->Error("Failed to open zip entry at index %d", static_cast<int>(i));
            success = false;
            break;
        }

        const char *entryName = zip_entry_name(zip);
        if (!entryName) {
            zip_entry_close(zip);
            continue;
        }

        std::string normalizedEntryName = NormalizePath(entryName);
        std::string outputPath = tempDir + "\\" + normalizedEntryName;

        // Create parent directories if needed
        fs::path outputFilePath(outputPath);
        try {
            if (outputFilePath.has_parent_path()) {
                fs::create_directories(outputFilePath.parent_path());
            }
        } catch (const fs::filesystem_error &e) {
            m_Engine->GetLogger()->Error("Failed to create directory for %s: %s", outputPath.c_str(), e.what());
            zip_entry_close(zip);
            success = false;
            break;
        }

        // Check if this is a directory entry
        if (zip_entry_isdir(zip)) {
            try {
                fs::create_directories(outputPath);
            } catch (const fs::filesystem_error &e) {
                m_Engine->GetLogger()->Error("Failed to create directory %s: %s", outputPath.c_str(), e.what());
                success = false;
            }
            zip_entry_close(zip);
            continue;
        }

        // Extract file content
        if (zip_entry_fread(zip, outputPath.c_str()) != 0) {
            m_Engine->GetLogger()->Error("Failed to extract file: %s", outputPath.c_str());
            success = false;
            zip_entry_close(zip);
            break;
        }

        zip_entry_close(zip);
    }

    zip_close(zip);

    if (!success) {
        m_Engine->GetLogger()->Error("Failed to extract zip project, cleaning up temp directory");
        RemoveDirectory(tempDir);
        return false;
    }

    m_Engine->GetLogger()->Info("Successfully extracted zip project to: %s", tempDir.c_str());
    return true;
}

bool ProjectManager::CompressProject(const std::string &projectPath, const std::string &zipPath) {
    m_Engine->GetLogger()->Info("Compressing project: %s to %s", projectPath.c_str(), zipPath.c_str());

    if (!ValidateProjectStructure(projectPath)) {
        m_Engine->GetLogger()->Error("Project structure validation failed: %s", projectPath.c_str());
        return false;
    }

    std::vector<std::pair<std::string, std::string>> files; // <full_path, relative_path>

    // Collect all files in the project directory
    try {
        for (const auto &entry : fs::recursive_directory_iterator(projectPath)) {
            if (entry.is_regular_file()) {
                std::string fullPath = entry.path().string();
                std::string relativePath = fs::relative(entry.path(), projectPath).string();

                // Convert backslashes to forward slashes for zip compatibility
                std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
                files.emplace_back(fullPath, relativePath);
            }
        }
    } catch (const fs::filesystem_error &e) {
        m_Engine->GetLogger()->Error("Error scanning project directory: %s", e.what());
        return false;
    }

    if (files.empty()) {
        m_Engine->GetLogger()->Error("No files found in project directory: %s", projectPath.c_str());
        return false;
    }

    // Create zip file
    struct zip_t *zip = zip_open(zipPath.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    if (!zip) {
        m_Engine->GetLogger()->Error("Failed to create zip file: %s", zipPath.c_str());
        return false;
    }

    bool success = true;
    for (const auto &[filePath, relativePath] : files) {
        if (zip_entry_open(zip, relativePath.c_str()) != 0) {
            m_Engine->GetLogger()->Error("Failed to open zip entry: %s", relativePath.c_str());
            success = false;
            break;
        }

        if (zip_entry_fwrite(zip, filePath.c_str()) != 0) {
            m_Engine->GetLogger()->Error("Failed to write file to zip: %s", filePath.c_str());
            success = false;
            zip_entry_close(zip);
            break;
        }

        zip_entry_close(zip);
    }

    zip_close(zip);

    if (success) {
        m_Engine->GetLogger()->Info("Successfully compressed project with %d files", static_cast<int>(files.size()));
    } else {
        fs::remove(zipPath); // Clean up failed zip
    }

    return success;
}

bool ProjectManager::ValidateZipProject(const std::string &zipPath) {
    struct zip_t *zip = zip_open(zipPath.c_str(), 0, 'r');
    if (!zip) {
        return false;
    }

    bool hasManifest = false;
    bool hasMainScript = false;

    ssize_t totalEntries = zip_entries_total(zip);
    for (ssize_t i = 0; i < totalEntries; ++i) {
        if (zip_entry_openbyindex(zip, i) != 0) {
            continue;
        }

        const char *entryName = zip_entry_name(zip);
        if (entryName) {
            std::string name = entryName;
            // Handle both forward and back slashes
            if (name == "manifest.lua" || name == ".\\manifest.lua" || name == "./manifest.lua") {
                hasManifest = true;
            } else if (name == "main.lua" || name == ".\\main.lua" || name == "./main.lua") {
                hasMainScript = true;
            }
        }

        zip_entry_close(zip);

        if (hasManifest && hasMainScript) {
            break;
        }
    }

    zip_close(zip);

    bool isValid = hasManifest && hasMainScript;
    if (!isValid) {
        m_Engine->GetLogger()->Warn("Zip project validation failed: %s (manifest: %s, main: %s)",
                                 zipPath.c_str(), hasManifest ? "yes" : "no", hasMainScript ? "yes" : "no");
    }

    return isValid;
}

bool ProjectManager::ReadFileFromZip(const std::string &zipPath, const std::string &fileName, std::string &content) {
    struct zip_t *zip = zip_open(zipPath.c_str(), 0, 'r');
    if (!zip) {
        m_Engine->GetLogger()->Error("Failed to open zip file: %s", zipPath.c_str());
        return false;
    }

    bool success = false;
    if (zip_entry_open(zip, fileName.c_str()) == 0) {
        void *buf = nullptr;
        size_t bufsize = 0;

        ssize_t result = zip_entry_read(zip, &buf, &bufsize);
        if (result > 0 && buf) {
            content.assign(static_cast<char *>(buf), bufsize);
            success = true;
        }

        if (buf) {
            free(buf);
        }

        zip_entry_close(zip);
    }

    zip_close(zip);

    if (!success) {
        m_Engine->GetLogger()->Error("Failed to read file '%s' from zip: %s", fileName.c_str(), zipPath.c_str());
    }

    return success;
}

void ProjectManager::SetCurrentProject(TASProject *project) {
    // Clean up previous project's temp directory if it was different
    if (m_CurrentProject && m_CurrentProject != project) {
        CleanupProjectTempDirectory(m_CurrentProject);
    }

    m_CurrentProject = project;
}

std::string ProjectManager::CreateTempDirectory(const std::string &baseName) {
    // Generate unique directory name
    auto now = std::chrono::steady_clock::now();
    auto timestamp = now.time_since_epoch().count();

    std::string tempDirName = baseName + "_" + std::to_string(timestamp);
    std::string tempDirPath = m_TempDir + tempDirName;

    // Ensure uniqueness
    int counter = 0;
    while (fs::exists(tempDirPath) && counter < 1000) {
        tempDirPath = m_TempDir + tempDirName + "_" + std::to_string(counter);
        counter++;
    }

    return tempDirPath;
}

bool ProjectManager::RemoveDirectory(const std::string &dirPath) {
    try {
        return fs::remove_all(dirPath) > 0;
    } catch (const fs::filesystem_error &e) {
        m_Engine->GetLogger()->Error("Failed to remove directory '%s': %s", dirPath.c_str(), e.what());
        return false;
    }
}

void ProjectManager::CleanupTempDirectories() {
    m_Engine->GetLogger()->Info("Cleaning up temporary directories");

    // Clean up project-specific temp directories
    for (auto it = m_ProjectTempDirectories.begin(); it != m_ProjectTempDirectories.end();) {
        const std::string &tempDir = it->second;
        if (fs::exists(tempDir)) {
            if (RemoveDirectory(tempDir)) {
                m_Engine->GetLogger()->Info("Cleaned up temp directory: %s", tempDir.c_str());
            }
        }
        it = m_ProjectTempDirectories.erase(it);
    }

    // Clean up legacy temp directories
    for (const auto &tempDir : m_TempDirectories) {
        if (fs::exists(tempDir)) {
            RemoveDirectory(tempDir);
        }
    }
    m_TempDirectories.clear();

    // Also clean up any orphaned temp files/directories
    try {
        for (const auto &entry : fs::directory_iterator(m_TempDir)) {
            if (entry.is_directory() || entry.path().filename().string().find("temp_") == 0) {
                fs::remove_all(entry.path());
            }
        }
    } catch (const fs::filesystem_error &e) {
        m_Engine->GetLogger()->Warn("Error during temp cleanup: %s", e.what());
    }
}

void ProjectManager::CleanupProjectTempDirectory(TASProject *project) {
    if (!project) return;

    auto it = m_ProjectTempDirectories.find(project);
    if (it != m_ProjectTempDirectories.end()) {
        const std::string &tempDir = it->second;
        if (fs::exists(tempDir)) {
            if (RemoveDirectory(tempDir)) {
                m_Engine->GetLogger()->Info("Cleaned up temp directory for project %s: %s",
                                         project->GetName().c_str(), tempDir.c_str());
            }
        }
        m_ProjectTempDirectories.erase(it);
    }
}

std::string ProjectManager::GetProjectNameFromZip(const std::string &zipPath) {
    std::string manifestContent;
    if (ReadFileFromZip(zipPath, "manifest.lua", manifestContent)) {
        // Quick and dirty parsing to extract name
        size_t namePos = manifestContent.find("name = \"");
        if (namePos != std::string::npos) {
            namePos += 8; // Length of "name = \""
            size_t endPos = manifestContent.find("\"", namePos);
            if (endPos != std::string::npos) {
                return manifestContent.substr(namePos, endPos - namePos);
            }
        }
    }

    // Fallback to filename without extension
    fs::path zipFilePath(zipPath);
    return zipFilePath.stem().string();
}

std::string ProjectManager::NormalizePath(const std::string &path) {
    std::string normalized = path;
    // Convert forward slashes to backslashes on Windows
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

bool ProjectManager::ValidateProjectStructure(const std::string &projectPath) {
    if (!fs::exists(projectPath) || !fs::is_directory(projectPath)) {
        return false;
    }

    // Check for required files
    std::string manifestPath = projectPath + "\\manifest.lua";
    std::string mainScriptPath = projectPath + "\\main.lua";

    return fs::exists(manifestPath) && fs::exists(mainScriptPath) &&
        fs::is_regular_file(manifestPath) && fs::is_regular_file(mainScriptPath);
}

sol::table ProjectManager::ParseManifestFile(const std::string &path) {
    try {
        auto &lua = m_Engine->GetLuaState();
        sol::table envTable = lua.create_table();
        envTable[sol::metatable_key] = lua.create_table_with(
            sol::meta_method::index, lua.globals()
        );

        sol::environment env(lua, envTable);

        sol::protected_function_result result = lua.safe_script_file(path, env);

        if (!result.valid()) {
            sol::error err = result;
            m_Engine->GetLogger()->Error("Error parsing manifest file '%s': %s", path.c_str(), err.what());
            return sol::table();
        }

        if (result.get_type() == sol::type::table) {
            return result.get<sol::table>();
        }

        m_Engine->GetLogger()->Warn("Manifest file '%s' did not return a table.", path.c_str());
        return sol::table();
    } catch (const sol::error &e) {
        m_Engine->GetLogger()->Error("Exception while parsing manifest '%s': %s", path.c_str(), e.what());
        return sol::table();
    }
}

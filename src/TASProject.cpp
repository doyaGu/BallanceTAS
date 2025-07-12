#include "TASProject.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Constructor for script-based projects
TASProject::TASProject(std::string projectPath, sol::table manifest)
    : m_ProjectPath(std::move(projectPath)), m_Manifest(std::move(manifest)), m_ProjectType(ProjectType::Script) {
    ParseManifest(m_Manifest);
}

// Constructor for record-based projects (.tas files)
TASProject::TASProject(std::string tasFilePath)
    : m_ProjectPath(std::move(tasFilePath)), m_ProjectType(ProjectType::Record) {
    ParseRecordProject(m_ProjectPath);
}

void TASProject::ParseManifest(const sol::table &manifest) {
    if (!manifest.valid()) {
        m_IsValid = false;
        return;
    }

    // Safely extract fields from the Lua table using sol2's `get_or`
    m_Name = manifest.get_or<std::string>("name", "Unnamed TAS");
    m_Author = manifest.get_or<std::string>("author", "Unknown");
    m_TargetLevel = manifest.get_or<std::string>("level", "");
    m_EntryScript = manifest.get_or<std::string>("entry_script", "main.lua");
    m_Description = manifest.get_or<std::string>("description", "No description.");
    m_UpdateRate = manifest.get_or<float>("update_rate", 132);

    // Parse legacy mode settings
    if (manifest["legacy_mode"].get_type() == sol::type::boolean) {
        m_LegacyMode = manifest.get_or("legacy_mode", false);
    } else if (manifest["legacy_mode"].get_type() == sol::type::string) {
        // Check for string values like "required"
        std::string legacyMode = manifest.get_or<std::string>("legacy_mode", "");
        if (legacyMode == "required") {
            m_LegacyMode = true;
        }
    }

    // A project is considered valid if it has the essential fields.
    if (!m_TargetLevel.empty() && !m_Name.empty() && !m_Author.empty()) {
        m_IsValid = true;
    }
}

void TASProject::ParseRecordProject(const std::string &tasFilePath) {
    // Validate that the .tas file exists
    if (!fs::exists(tasFilePath) || !fs::is_regular_file(tasFilePath)) {
        m_IsValid = false;
        return;
    }

    // Extract project name from filename (without extension)
    fs::path filePath(tasFilePath);
    m_Name = filePath.stem().string();

    // Set default values for record projects
    m_Author = "Unknown (Legacy Record)";
    m_Description = "Legacy TAS record file";
    m_TargetLevel = "";    // Will be determined during playback if possible
    m_UpdateRate = 132.0f; // Standard Physics rate

    // Record projects ALWAYS require legacy mode
    m_LegacyMode = true;

    // Try to parse some basic info from the file if possible
    try {
        std::ifstream file(tasFilePath, std::ios::binary);
        if (file) {
            // Read just the header to verify it's a valid .tas file
            uint32_t uncompressedSize;
            file.read(reinterpret_cast<char *>(&uncompressedSize), sizeof(uncompressedSize));

            if (file.gcount() == sizeof(uncompressedSize) && uncompressedSize > 0) {
                // File appears to be valid

                // Update description with file size info
                auto fileSize = fs::file_size(tasFilePath);
                auto frameCount = uncompressedSize / 8; // Each FrameData is 8 bytes

                m_Description = "Legacy TAS record (" + std::to_string(frameCount) + " frames, " +
                    std::to_string(fileSize) + " bytes)";

                m_IsValid = true;
            }
        }
    } catch (const std::exception &) {
        // If we can't read the file, mark as invalid
        m_IsValid = false;
    }
}

std::string TASProject::GetRecordFilePath() const {
    if (IsRecordProject()) {
        return m_ProjectPath; // For record projects, the project path IS the .tas file
    }
    return ""; // Script projects don't have record files
}

std::string TASProject::GetEntryScriptPath(const std::string &executionBasePath) const {
    if (IsRecordProject()) {
        return ""; // Record projects don't have entry scripts
    }
    return GetProjectFilePath(m_EntryScript, executionBasePath);
}

std::string TASProject::GetProjectFilePath(const std::string &fileName, const std::string &executionBasePath) const {
    if (IsRecordProject()) {
        return ""; // Record projects don't have project files
    }

    std::string basePath;

    if (!executionBasePath.empty()) {
        // Use provided execution base path (for zip projects)
        basePath = executionBasePath;
    } else if (!m_ExecutionBasePath.empty()) {
        // Use stored execution base path
        basePath = m_ExecutionBasePath;
    } else {
        // Use original project path (for directory projects)
        basePath = m_ProjectPath;
    }

    // Handle path separator properly
    if (!basePath.empty() && basePath.back() != '\\' && basePath.back() != '/') {
        basePath += "\\";
    }

    return basePath + fileName;
}

bool TASProject::IsCompatibleWithSettings(bool currentLegacyMode) const {
    // Check required settings
    if (m_LegacyMode && !currentLegacyMode) {
        return false;
    }

    return true;
}

std::string TASProject::GetCompatibilityMessage(bool currentLegacyMode) const {
    // Build incompatibility message
    std::vector<std::string> requirements;

    if (m_LegacyMode && !currentLegacyMode) {
        requirements.emplace_back("legacy mode required");
    }

    if (requirements.empty()) {
        return ""; // Compatible
    }

    std::string message = "Incompatible: ";
    for (size_t i = 0; i < requirements.size(); ++i) {
        if (i > 0) message += ", ";
        message += requirements[i];
    }

    return message;
}

std::vector<std::string> TASProject::GetRequirements() const {
    std::vector<std::string> requirements;

    if (m_LegacyMode) {
        requirements.emplace_back("Legacy Mode");
    }

    return requirements;
}

#include "TASProject.h"

TASProject::TASProject(std::string projectPath, sol::table manifest)
    : m_ProjectPath(std::move(projectPath)), m_Manifest(std::move(manifest)) {
    ParseManifest(m_Manifest);
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
    m_DeltaTime = 1.0f / m_UpdateRate * 1000.0f; // Convert to milliseconds

    // A project is considered valid if it has the essential fields.
    if (!m_TargetLevel.empty() && !m_Name.empty() && !m_Author.empty()) {
        m_IsValid = true;
    }
}

std::string TASProject::GetEntryScriptPath(const std::string &executionBasePath) const {
    return GetProjectFilePath(m_EntryScript, executionBasePath);
}

std::string TASProject::GetProjectFilePath(const std::string &fileName, const std::string &executionBasePath) const {
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

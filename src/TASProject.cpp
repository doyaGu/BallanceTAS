#include "TASProject.h"

#include <filesystem>
#include <fstream>

#include <CKGlobals.h>

#include "RecordPlayer.h"

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
    m_Author = "Unknown (Record)";
    m_Description = "TAS record file";
    m_TargetLevel = "";    // Will be determined during playback if possible
    m_UpdateRate = 132.0f; // Standard Physics rate

    // Try to parse timing and basic info from the file
    try {
        std::ifstream file(tasFilePath, std::ios::binary);
        if (!file.is_open()) {
            m_IsValid = false;
            return;
        }

        // Read the 4-byte header for uncompressed size
        uint32_t uncompressedSize;
        file.read(reinterpret_cast<char *>(&uncompressedSize), sizeof(uncompressedSize));
        if (file.gcount() != sizeof(uncompressedSize)) {
            m_IsValid = false;
            return;
        }

        if (uncompressedSize == 0) {
            // Empty file - technically valid but no useful data
            m_Description = "Empty TAS record file";
            m_IsValid = true;
            return;
        }

        // Validate that uncompressed size makes sense
        const size_t frameDataSize = sizeof(float) + sizeof(int); // deltaTime + keyStates
        if (uncompressedSize % frameDataSize != 0) {
            m_IsValid = false;
            return;
        }

        // Calculate frame count
        size_t frameCount = uncompressedSize / frameDataSize;

        // Read the compressed payload
        file.seekg(0, std::ios::end);
        std::streampos fileSize = file.tellg();
        file.seekg(sizeof(uncompressedSize), std::ios::beg);

        size_t compressedSize = static_cast<size_t>(fileSize) - sizeof(uncompressedSize);
        if (compressedSize == 0) {
            m_IsValid = false;
            return;
        }

        std::vector<char> compressedData(compressedSize);
        file.read(compressedData.data(), compressedSize);
        if (static_cast<size_t>(file.gcount()) != compressedSize) {
            m_IsValid = false;
            return;
        }
        file.close();

        // Decompress the data
        char *uncompressedData = CKUnPackData(static_cast<int>(uncompressedSize), compressedData.data(), compressedSize);
        if (!uncompressedData) {
            m_IsValid = false;
            return;
        }

        // Parse timing information from the frames
        if (frameCount > 0) {
            // Cast to frame data array
            const auto *frames = reinterpret_cast<const RecordFrameData *>(uncompressedData);

            bool hasConstantDeltaTime = true;
            float initialDeltaTime = frames[0].deltaTime;
            for (size_t i = 1; i < frameCount; ++i) {
                float deltaTime = frames[i].deltaTime;
                if (deltaTime != initialDeltaTime) {
                    // If delta time varies from the first frame, we need to check consistency
                    hasConstantDeltaTime = false;
                }
            }

            m_UpdateRate = 1000.0f / initialDeltaTime; // Convert from ms to Hz

            // Store delta time consistency information
            m_HasConstantDeltaTime = hasConstantDeltaTime;
        } else {
            m_HasConstantDeltaTime = true; // Empty record is technically constant
        }

        // Clean up decompressed data
        CKDeletePointer(uncompressedData);

        // Update description
        std::ostringstream desc;
        desc << "TAS record (" << frameCount << " frames)";
        m_Description = desc.str();

        m_IsValid = true;
    } catch (const std::exception &) {
        // If we can't read the file properly, mark as invalid
        m_IsValid = false;
        m_HasConstantDeltaTime = false;
        m_Description = "Invalid TAS record file";
    }
}

std::string TASProject::GetFileName() const {
    return fs::path(m_ProjectPath).filename().string();
}

std::string TASProject::GetFileNameWithoutExtension() const {
    return fs::path(m_ProjectPath).stem().string();
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


std::string TASProject::GetTranslationCompatibilityMessage() const {
    if (!IsRecordProject()) {
        return "Not a record project";
    }

    if (!IsValid()) {
        return "Invalid record file";
    }

    if (!m_HasConstantDeltaTime) {
        std::ostringstream msg;
        msg << "Variable timing detected. "
            << "Only records with constant delta time can be translated to scripts.";
        return msg.str();
    }

    return "Compatible - constant timing detected";
}

std::vector<std::string> TASProject::GetRequirements() const {
    std::vector<std::string> requirements;

    // Add translation-specific requirements for record projects
    if (IsRecordProject() && IsValid()) {
        if (!m_HasConstantDeltaTime) {
            requirements.emplace_back("Variable Timing (Translation Not Recommended)");
        }
    }

    return requirements;
}

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
 */
class TASProject {
public:
    // A project is defined by its root directory path.
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
    std::string GetEntryScriptPath() const;

    bool IsValid() const { return m_IsValid; }

private:
    void ParseManifest(const sol::table &manifest);

    std::string m_ProjectPath;
    sol::table m_Manifest; // Keep a copy of the raw manifest table

    // Parsed and cached data
    std::string m_Name = "Unnamed TAS";
    std::string m_Author = "Unknown";
    std::string m_Description = "No description.";
    std::string m_EntryScript = "main.lua";
    std::string m_TargetLevel;
    float m_UpdateRate = 132.0f; // Default to 132 = 66 * 2 (game's physics update rate)
    float m_DeltaTime = 1 / 132.0f * 1000;

    bool m_IsValid = false;
};

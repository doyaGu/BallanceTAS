#pragma once

#include <cstddef>
#include <string>

class TASEngine;

class HarnessRuntime {
public:
    explicit HarnessRuntime(TASEngine *engine);

    static bool IsRequested();

    bool Initialize();
    void Tick();
    bool Complete(bool passed, const std::string &message);

    bool IsActive() const { return m_Active; }
    bool IsCompleted() const { return m_Completed; }
    const std::string &GetProjectName() const { return m_Project; }

private:
    bool WriteResult(bool passed, const std::string &message);

    TASEngine *m_Engine;
    bool m_Active = false;
    bool m_Started = false;
    bool m_Completed = false;
    std::string m_RequestPath;
    std::string m_ResultPath;
    std::string m_Project;
    size_t m_MaxTicks = 20000;
    size_t m_StartTick = 0;
};

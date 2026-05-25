#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "HookManager.h"
#include "DeterminismTrace.h"
#include "Result.h"

class ServiceProvider;
class GameInterface;
class CKIpionManager;

class DeterminismVerifier {
public:
    enum class Mode { Idle, Recording, Verifying };

    struct Status {
        Mode mode = Mode::Idle;
        uint64_t ticksProcessed = 0;
        bool diverged = false;
        uint64_t divergenceTick = 0;
        uint64_t lastHash = 0;
        std::string currentPath;
    };

    explicit DeterminismVerifier(ServiceProvider &services);
    ~DeterminismVerifier();

    DeterminismVerifier(const DeterminismVerifier &) = delete;
    DeterminismVerifier &operator=(const DeterminismVerifier &) = delete;

    Result<void> StartRecording(const std::string &outputPath);
    Result<void> StartVerification(const std::string &referencePath);
    void Stop();

    Mode GetMode() const { return m_Mode; }
    Status GetStatus() const;
    uint64_t GetCurrentHash() const { return m_LastHash; }

    void ProcessTick();

    static Result<tas::determinism::TraceDiff> OfflineDiff(const std::string &pathA, const std::string &pathB);

private:
    Result<void> WriteHeader(const std::string &levelName);
    Result<void> ReadAndValidateHeader();
    void FinalizeFile();

    ServiceProvider &m_Services;

    // Resolved lazily
    GameInterface *m_GameInterface = nullptr;
    HookManager *m_HookManager = nullptr;
    bool m_LayoutValidated = false;

    // State
    Mode m_Mode = Mode::Idle;
    uint64_t m_TickCounter = 0;
    uint64_t m_LastHash = 0;
    bool m_Diverged = false;
    uint64_t m_DivergenceTick = 0;
    std::string m_CurrentPath;

    // File I/O
    std::fstream m_File;
    std::streampos m_TickCountPos;

    // Reference data for verification mode
    std::vector<uint64_t> m_ReferenceHashes;

    // Hook callback (auto-unregisters on destruction)
    ScopedCallback m_TickCallback;

    // File format constants
    static constexpr uint32_t kMagic = 0x53415442; // "BTAS" little-endian
    static constexpr uint32_t kVersion = 1;
    static constexpr uint32_t kFlagNone = 0;
};

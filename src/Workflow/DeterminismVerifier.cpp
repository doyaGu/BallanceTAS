#include "DeterminismVerifier.h"

#include "DeterminismTrace.h"
#include "GameInterface.h"
#include "Logger.h"
#include "PhysicsWorldSnapshot.h"
#include "ServiceContainer.h"

// ============================================================================
// Construction / Destruction
// ============================================================================

DeterminismVerifier::DeterminismVerifier(ServiceProvider &services)
    : m_Services(services) {}

DeterminismVerifier::~DeterminismVerifier() {
    Stop();
}

// ============================================================================
// Lazy Resolution
// ============================================================================

static GameInterface *ResolveGameInterface(ServiceProvider &sp) {
    return sp.Resolve<GameInterface>();
}

static HookManager *ResolveHookManager(ServiceProvider &sp) {
    return sp.Resolve<HookManager>();
}

// ============================================================================
// Start Recording
// ============================================================================

Result<void> DeterminismVerifier::StartRecording(const std::string &outputPath) {
    if (m_Mode != Mode::Idle) {
        return Result<void>::Error("DeterminismVerifier is already active", "already_active");
    }

    if (!m_GameInterface) {
        m_GameInterface = ResolveGameInterface(m_Services);
    }
    if (!m_GameInterface) {
        return Result<void>::Error("GameInterface not available", "no_game_interface");
    }

    if (!m_HookManager) {
        m_HookManager = ResolveHookManager(m_Services);
    }
    if (!m_HookManager) {
        return Result<void>::Error("HookManager not available", "no_hook_manager");
    }

    CKIpionManager *ipion = m_GameInterface->GetIpionManager();
    if (!ipion) {
        return Result<void>::Error("CKIpionManager not available", "no_ipion_manager");
    }

    // Validate physics layout on first use
    if (!m_LayoutValidated) {
        if (!ValidatePhysicsLayout(ipion)) {
            return Result<void>::Error(
                "Physics layout validation failed — struct offsets may be incorrect",
                "layout_validation_failed");
        }
        m_LayoutValidated = true;
    }

    // Open output file
    m_File.open(outputPath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!m_File.is_open()) {
        return Result<void>::Error("Failed to open output file: " + outputPath, "file_open_failed");
    }

    std::string levelName = m_GameInterface->GetMapName();
    auto headerResult = WriteHeader(levelName);
    if (headerResult.IsError()) {
        m_File.close();
        return headerResult;
    }

    m_Mode = Mode::Recording;
    m_TickCounter = 0;
    m_LastHash = 0;
    m_Diverged = false;
    m_DivergenceTick = 0;
    m_CurrentPath = outputPath;

    // Register post-tick callback for automatic capture
    m_TickCallback = m_HookManager->RegisterPostTickCallback(
        [this](CKTimeManager *) { ProcessTick(); });

    Log::Info("DeterminismVerifier: Recording started -> %s", outputPath.c_str());
    return Result<void>::Ok();
}

// ============================================================================
// Start Verification
// ============================================================================

Result<void> DeterminismVerifier::StartVerification(const std::string &referencePath) {
    if (m_Mode != Mode::Idle) {
        return Result<void>::Error("DeterminismVerifier is already active", "already_active");
    }

    if (!m_GameInterface) {
        m_GameInterface = ResolveGameInterface(m_Services);
    }
    if (!m_GameInterface) {
        return Result<void>::Error("GameInterface not available", "no_game_interface");
    }

    if (!m_HookManager) {
        m_HookManager = ResolveHookManager(m_Services);
    }
    if (!m_HookManager) {
        return Result<void>::Error("HookManager not available", "no_hook_manager");
    }

    CKIpionManager *ipion = m_GameInterface->GetIpionManager();
    if (!ipion) {
        return Result<void>::Error("CKIpionManager not available", "no_ipion_manager");
    }

    if (!m_LayoutValidated) {
        if (!ValidatePhysicsLayout(ipion)) {
            return Result<void>::Error(
                "Physics layout validation failed", "layout_validation_failed");
        }
        m_LayoutValidated = true;
    }

    // Open reference file and load all hashes
    m_File.open(referencePath, std::ios::binary | std::ios::in);
    if (!m_File.is_open()) {
        return Result<void>::Error("Failed to open reference file: " + referencePath,
                                   "file_open_failed");
    }

    auto headerResult = ReadAndValidateHeader();
    if (headerResult.IsError()) {
        m_File.close();
        return headerResult;
    }

    // Read all (tick, hash) records
    m_ReferenceHashes.clear();
    while (m_File.good() && !m_File.eof()) {
        uint64_t tick = 0, hash = 0;
        m_File.read(reinterpret_cast<char *>(&tick), sizeof(tick));
        m_File.read(reinterpret_cast<char *>(&hash), sizeof(hash));
        if (m_File.gcount() == sizeof(hash)) {
            m_ReferenceHashes.push_back(hash);
        }
    }
    m_File.close();

    if (m_ReferenceHashes.empty()) {
        return Result<void>::Error("Reference file contains no tick records", "empty_reference");
    }

    m_Mode = Mode::Verifying;
    m_TickCounter = 0;
    m_LastHash = 0;
    m_Diverged = false;
    m_DivergenceTick = 0;
    m_CurrentPath = referencePath;

    m_TickCallback = m_HookManager->RegisterPostTickCallback(
        [this](CKTimeManager *) { ProcessTick(); });

    Log::Info("DeterminismVerifier: Verification started with %zu reference ticks",
              m_ReferenceHashes.size());
    return Result<void>::Ok();
}

// ============================================================================
// Stop
// ============================================================================

void DeterminismVerifier::Stop() {
    if (m_Mode == Mode::Idle) {
        return;
    }

    // Unregister hook callback
    m_TickCallback.Reset();

    if (m_Mode == Mode::Recording && m_File.is_open()) {
        FinalizeFile();
        m_File.close();
        Log::Info("DeterminismVerifier: Recording stopped (%llu ticks)",
                  static_cast<unsigned long long>(m_TickCounter));
    }

    if (m_Mode == Mode::Verifying) {
        if (m_Diverged) {
            Log::Warn("DeterminismVerifier: Verification stopped — DIVERGED at tick %llu",
                      static_cast<unsigned long long>(m_DivergenceTick));
        } else {
            Log::Info("DeterminismVerifier: Verification stopped — %llu ticks matched",
                      static_cast<unsigned long long>(m_TickCounter));
        }
        m_ReferenceHashes.clear();
    }

    m_Mode = Mode::Idle;
}

// ============================================================================
// Status
// ============================================================================

DeterminismVerifier::Status DeterminismVerifier::GetStatus() const {
    return {m_Mode, m_TickCounter, m_Diverged, m_DivergenceTick, m_LastHash, m_CurrentPath};
}

// ============================================================================
// ProcessTick
// ============================================================================

void DeterminismVerifier::ProcessTick() {
    if (m_Mode == Mode::Idle) return;

    if (!m_GameInterface) return;

    CKIpionManager *ipion = m_GameInterface->GetIpionManager();
    if (!ipion) return;

    PhysicsWorldSnapshot snapshot = CaptureWorldSnapshot(ipion);
    uint64_t hash = snapshot.ComputeHash();
    m_LastHash = hash;

    uint64_t tick = m_TickCounter;
    m_TickCounter++;

    if (m_Mode == Mode::Recording) {
        // Write (tick, hash) record
        m_File.write(reinterpret_cast<const char *>(&tick), sizeof(tick));
        m_File.write(reinterpret_cast<const char *>(&hash), sizeof(hash));
    } else if (m_Mode == Mode::Verifying) {
        if (tick < m_ReferenceHashes.size()) {
            uint64_t refHash = m_ReferenceHashes[tick];
            if (hash != refHash && !m_Diverged) {
                m_Diverged = true;
                m_DivergenceTick = tick;
                Log::Warn("DeterminismVerifier: DIVERGENCE at tick %llu "
                          "(live=0x%016llX, ref=0x%016llX)",
                          static_cast<unsigned long long>(tick),
                          static_cast<unsigned long long>(hash),
                          static_cast<unsigned long long>(refHash));
            }
        }
    }
}

// ============================================================================
// File Format Helpers
// ============================================================================

Result<void> DeterminismVerifier::WriteHeader(const std::string &levelName) {
    // Magic
    m_File.write(reinterpret_cast<const char *>(&kMagic), sizeof(kMagic));

    // Version
    m_File.write(reinterpret_cast<const char *>(&kVersion), sizeof(kVersion));

    // Flags
    uint32_t flags = kFlagNone;
    m_File.write(reinterpret_cast<const char *>(&flags), sizeof(flags));

    // Tick count placeholder (updated on finalize)
    m_TickCountPos = m_File.tellp();
    uint64_t tickCount = 0;
    m_File.write(reinterpret_cast<const char *>(&tickCount), sizeof(tickCount));

    // Level name (length-prefixed)
    uint16_t nameLen = static_cast<uint16_t>(levelName.size());
    m_File.write(reinterpret_cast<const char *>(&nameLen), sizeof(nameLen));
    if (nameLen > 0) {
        m_File.write(levelName.data(), nameLen);
    }

    if (!m_File.good()) {
        return Result<void>::Error("Failed to write file header", "write_failed");
    }

    return Result<void>::Ok();
}

Result<void> DeterminismVerifier::ReadAndValidateHeader() {
    uint32_t magic = 0;
    m_File.read(reinterpret_cast<char *>(&magic), sizeof(magic));
    if (magic != kMagic) {
        return Result<void>::Error("Invalid file magic (not a BTAS log)", "invalid_magic");
    }

    uint32_t version = 0;
    m_File.read(reinterpret_cast<char *>(&version), sizeof(version));
    if (version != kVersion) {
        return Result<void>::Error("Unsupported file version: " + std::to_string(version),
                                   "unsupported_version");
    }

    uint32_t flags = 0;
    m_File.read(reinterpret_cast<char *>(&flags), sizeof(flags));

    uint64_t tickCount = 0;
    m_File.read(reinterpret_cast<char *>(&tickCount), sizeof(tickCount));

    uint16_t nameLen = 0;
    m_File.read(reinterpret_cast<char *>(&nameLen), sizeof(nameLen));
    if (nameLen > 0) {
        std::string levelName(nameLen, '\0');
        m_File.read(levelName.data(), nameLen);
    }

    if (!m_File.good()) {
        return Result<void>::Error("Failed to read file header", "read_failed");
    }

    return Result<void>::Ok();
}

void DeterminismVerifier::FinalizeFile() {
    // Seek back to tick_count field and write actual count
    m_File.seekp(m_TickCountPos);
    m_File.write(reinterpret_cast<const char *>(&m_TickCounter), sizeof(m_TickCounter));
    m_File.flush();
}

// ============================================================================
// Offline Diff
// ============================================================================

Result<tas::determinism::TraceDiff> DeterminismVerifier::OfflineDiff(const std::string &pathA,
                                                                      const std::string &pathB) {
    auto result = tas::determinism::OfflineDiff(pathA, pathB);
    if (result.IsError()) {
        return result;
    }

    const auto diff = result.Unwrap();
    Log::Info("DeterminismVerifier::OfflineDiff:");
    Log::Info("  File A: %zu ticks", diff.leftTicks);
    Log::Info("  File B: %zu ticks", diff.rightTicks);
    Log::Info("  Compared: %zu ticks", diff.comparedTicks);
    if (diff.identical) {
        Log::Info("  Result: IDENTICAL");
    } else {
        Log::Info("  First divergence: tick %zu", diff.firstDivergenceTick);
        Log::Info("  Divergent ticks: %zu / %zu", diff.divergentTicks, diff.comparedTicks);
    }
    return Result<tas::determinism::TraceDiff>::Ok(diff);
}

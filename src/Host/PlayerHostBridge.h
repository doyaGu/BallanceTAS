#pragma once

#include <cstdint>
#include <string>

#include "EventBus.h"
#include "TasHostApi.h"

class PlaybackService;
class ProjectManager;
class RecordPlayer;
class RuntimeSession;
class TASEngine;
class TASProject;

class PlayerHostBridge {
public:
    explicit PlayerHostBridge(TASEngine *engine);
    ~PlayerHostBridge();

    PlayerHostBridge(const PlayerHostBridge &) = delete;
    PlayerHostBridge &operator=(const PlayerHostBridge &) = delete;

    static bool IsRequested();

    bool Initialize();
    void Shutdown();
    /// Per-frame check that turns an early stop into a FAILED status for the host.
    void Tick();

private:
    static int BALLANCE_TAS_HOST_CALL QueryStatusThunk(void *runtime, BallanceTasRuntimeStatus *status);
    int QueryStatus(BallanceTasRuntimeStatus *status);
    bool DiscoverHost();
    void Fail(std::string message);
    void OnPlaybackCompleted(int playbackType);
    void OnLevelFinish();

    TASEngine *m_Engine = nullptr;
    const BallanceTasHostApi *m_HostApi = nullptr;
    PlaybackService *m_Playback = nullptr;
    RecordPlayer *m_RecordPlayer = nullptr;
    RuntimeSession *m_Session = nullptr;
    TASProject *m_Project = nullptr;
    BallanceTasHostLaunchSpec m_LaunchSpec{};
    BallanceTasRuntimeCallbacks m_Callbacks{};
    ScopedSubscription m_CompletionSubscription;
    ScopedSubscription m_LevelFinishSubscription;
    std::string m_Target;
    std::string m_ProjectName;
    std::string m_Message;
    uint32_t m_ProjectType = BALLANCE_TAS_PROJECT_UNKNOWN;
    uint64_t m_CompletedFrames = 0;
    bool m_Registered = false;
    bool m_PlaybackArmed = false;
    bool m_Completed = false;
    bool m_Failed = false;
};

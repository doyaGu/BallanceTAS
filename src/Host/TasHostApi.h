#ifndef PLAYER_TASHOSTAPI_H
#define PLAYER_TASHOSTAPI_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define BALLANCE_TAS_HOST_CALL __cdecl
#else
#define BALLANCE_TAS_HOST_CALL
#endif

#define BALLANCE_TAS_HOST_ABI_VERSION 1u
#define BALLANCE_TAS_HOST_UNKNOWN_FRAME_COUNT UINT64_MAX

enum BallanceTasRuntimePhase {
    BALLANCE_TAS_RUNTIME_WAITING = 1,
    BALLANCE_TAS_RUNTIME_ARMED = 2,
    BALLANCE_TAS_RUNTIME_PLAYING = 3,
    BALLANCE_TAS_RUNTIME_COMPLETED = 4,
    BALLANCE_TAS_RUNTIME_FAILED = 5
};

enum BallanceTasProjectType {
    BALLANCE_TAS_PROJECT_UNKNOWN = 0,
    BALLANCE_TAS_PROJECT_LUA = 1,
    BALLANCE_TAS_PROJECT_RECORD = 2
};

struct BallanceTasHostLaunchSpec {
    uint32_t size;
    uint32_t abi_version;
    const char *target_utf8;
    uint32_t start_paused;
    uint32_t exit_on_complete;
};

struct BallanceTasRuntimeStatus {
    uint32_t size;
    uint32_t abi_version;
    uint32_t phase;
    uint32_t project_type;
    const char *project_name_utf8;
    uint64_t consumed_frames;
    uint64_t total_frames;
    float next_delta_ms;
    const char *message_utf8;
};

typedef int(BALLANCE_TAS_HOST_CALL *BallanceTasQueryStatusFn)(void *runtime, BallanceTasRuntimeStatus *status);

struct BallanceTasRuntimeCallbacks {
    uint32_t size;
    uint32_t abi_version;
    BallanceTasQueryStatusFn query_status;
};

struct BallanceTasHostApi {
    uint32_t size;
    uint32_t abi_version;
    int(BALLANCE_TAS_HOST_CALL *get_launch_spec)(BallanceTasHostLaunchSpec *spec);
    int(BALLANCE_TAS_HOST_CALL *register_runtime)(const BallanceTasRuntimeCallbacks *callbacks, void *runtime);
    void(BALLANCE_TAS_HOST_CALL *unregister_runtime)(void *runtime);
};

typedef const BallanceTasHostApi *(BALLANCE_TAS_HOST_CALL *BallanceTasHostGetApiFn)(uint32_t requested_version);

#ifdef __cplusplus
static_assert(offsetof(BallanceTasHostLaunchSpec, target_utf8) == 8, "launch ABI drift");
static_assert(offsetof(BallanceTasRuntimeStatus, consumed_frames) % 8 == 0, "status ABI drift");
static_assert(offsetof(BallanceTasRuntimeCallbacks, query_status) == 8, "callbacks ABI drift");
static_assert(offsetof(BallanceTasHostApi, get_launch_spec) == 8, "host API drift");
#endif

#endif

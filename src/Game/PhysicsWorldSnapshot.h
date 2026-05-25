#pragma once

#include <cstdint>
#include <vector>

class CKIpionManager;

// ============================================================================
// FNV-1a 64-bit Hasher
// ============================================================================

class FNV1aHasher {
public:
    void Feed(const void *data, size_t len) {
        const uint8_t *bytes = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < len; ++i) {
            m_Hash ^= bytes[i];
            m_Hash *= kFNVPrime;
        }
    }

    template <typename T>
    void Feed(const T &value) {
        Feed(&value, sizeof(value));
    }

    uint64_t Finalize() const { return m_Hash; }

private:
    static constexpr uint64_t kFNVOffsetBasis = 14695981039346656037ULL;
    static constexpr uint64_t kFNVPrime = 1099511628211ULL;
    uint64_t m_Hash = kFNVOffsetBasis;
};

// ============================================================================
// Per-Core Physics State
// ============================================================================

struct CoreState {
    // IVP_U_Point pos_world_f_core_last_psi (3x double)
    double pos[3];

    // IVP_U_Quat q_world_f_core_last_psi (4x double)
    double q_last_psi[4];

    // IVP_U_Quat q_world_f_core_next_psi (4x double)
    double q_next_psi[4];

    // IVP_U_Float_Point speed (3x float)
    float speed[3];

    // IVP_U_Float_Point rot_speed (3x float)
    float rot_speed[3];

    // IVP_U_Float_Point speed_change (3x float)
    float speed_change[3];

    // IVP_U_Float_Point rot_speed_change (3x float)
    float rot_speed_change[3];

    // IVP_U_Float_Point delta_world_f_core_psis (3x float)
    float delta_psis[3];

    // IVP_Movement_Type (stored as uint8_t for canonical serialization)
    uint8_t movement_state;

    // IVP_FLOAT i_delta_time
    float i_delta_time;

    // IVP_Time time_of_last_psi
    double time_of_last_psi;

    void FeedHash(FNV1aHasher &hasher) const;
};

// ============================================================================
// Environment State
// ============================================================================

struct EnvironmentState {
    // IVP_Environment timing
    double current_time;
    double time_of_next_psi;
    double time_of_last_psi;
    short next_movement_check;
    int current_time_code;

    // RNG seeds
    int ivp_seed;
    int qh_seed;

    // CKIpionManager
    float delta_time;
    float physics_delta_time;

    void FeedHash(FNV1aHasher &hasher) const;
};

// ============================================================================
// Full Physics World Snapshot
// ============================================================================

struct PhysicsWorldSnapshot {
    EnvironmentState environment;
    std::vector<CoreState> cores;

    uint64_t ComputeHash() const;
};

// ============================================================================
// Capture & Validation
// ============================================================================

PhysicsWorldSnapshot CaptureWorldSnapshot(CKIpionManager *ipionManager);

bool ValidatePhysicsLayout(CKIpionManager *ipionManager);

#include "PhysicsWorldSnapshot.h"
#include "physics_RT.h"
#include "Logger.h"

#include <cmath>
#include <cstring>
#include <unordered_set>

// ============================================================================
// CoreState
// ============================================================================

void CoreState::FeedHash(FNV1aHasher &hasher) const {
    hasher.Feed(pos, sizeof(pos));
    hasher.Feed(q_last_psi, sizeof(q_last_psi));
    hasher.Feed(q_next_psi, sizeof(q_next_psi));
    hasher.Feed(speed, sizeof(speed));
    hasher.Feed(rot_speed, sizeof(rot_speed));
    hasher.Feed(speed_change, sizeof(speed_change));
    hasher.Feed(rot_speed_change, sizeof(rot_speed_change));
    hasher.Feed(delta_psis, sizeof(delta_psis));
    hasher.Feed(movement_state);
    hasher.Feed(i_delta_time);
    hasher.Feed(time_of_last_psi);
}

// ============================================================================
// EnvironmentState
// ============================================================================

void EnvironmentState::FeedHash(FNV1aHasher &hasher) const {
    hasher.Feed(current_time);
    hasher.Feed(time_of_next_psi);
    hasher.Feed(time_of_last_psi);
    hasher.Feed(next_movement_check);
    hasher.Feed(current_time_code);
    hasher.Feed(ivp_seed);
    hasher.Feed(qh_seed);
    hasher.Feed(delta_time);
    hasher.Feed(physics_delta_time);
}

// ============================================================================
// PhysicsWorldSnapshot
// ============================================================================

uint64_t PhysicsWorldSnapshot::ComputeHash() const {
    FNV1aHasher hasher;
    environment.FeedHash(hasher);
    for (const auto &core : cores) {
        core.FeedHash(hasher);
    }
    return hasher.Finalize();
}

// ============================================================================
// Capture from live CKIpionManager
// ============================================================================

static CoreState CaptureCoreState(const IVP_Core *core) {
    CoreState state{};

    // Position (IVP_U_Point — 3x double, skip hesse_val)
    state.pos[0] = core->pos_world_f_core_last_psi.k[0];
    state.pos[1] = core->pos_world_f_core_last_psi.k[1];
    state.pos[2] = core->pos_world_f_core_last_psi.k[2];

    // Quaternion last PSI
    state.q_last_psi[0] = core->q_world_f_core_last_psi.x;
    state.q_last_psi[1] = core->q_world_f_core_last_psi.y;
    state.q_last_psi[2] = core->q_world_f_core_last_psi.z;
    state.q_last_psi[3] = core->q_world_f_core_last_psi.w;

    // Quaternion next PSI
    state.q_next_psi[0] = core->q_world_f_core_next_psi.x;
    state.q_next_psi[1] = core->q_world_f_core_next_psi.y;
    state.q_next_psi[2] = core->q_world_f_core_next_psi.z;
    state.q_next_psi[3] = core->q_world_f_core_next_psi.w;

    // Velocities (IVP_U_Float_Point — 3x float, skip hesse_val)
    state.speed[0] = core->speed.k[0];
    state.speed[1] = core->speed.k[1];
    state.speed[2] = core->speed.k[2];

    state.rot_speed[0] = core->rot_speed.k[0];
    state.rot_speed[1] = core->rot_speed.k[1];
    state.rot_speed[2] = core->rot_speed.k[2];

    // Async push accumulators
    state.speed_change[0] = core->speed_change.k[0];
    state.speed_change[1] = core->speed_change.k[1];
    state.speed_change[2] = core->speed_change.k[2];

    state.rot_speed_change[0] = core->rot_speed_change.k[0];
    state.rot_speed_change[1] = core->rot_speed_change.k[1];
    state.rot_speed_change[2] = core->rot_speed_change.k[2];

    // Delta position
    state.delta_psis[0] = core->delta_world_f_core_psis.k[0];
    state.delta_psis[1] = core->delta_world_f_core_psis.k[1];
    state.delta_psis[2] = core->delta_world_f_core_psis.k[2];

    // Movement state (enum stored as bitfield, read as uint8_t)
    state.movement_state = static_cast<uint8_t>(core->movement_state);

    // Inverse delta time
    state.i_delta_time = core->i_delta_time;

    // Time of last PSI
    state.time_of_last_psi = core->time_of_last_psi.get_seconds();

    return state;
}

PhysicsWorldSnapshot CaptureWorldSnapshot(CKIpionManager *ipionManager) {
    PhysicsWorldSnapshot snapshot{};

    if (!ipionManager) {
        return snapshot;
    }

    IVP_Environment *env = ipionManager->GetEnvironment();
    if (!env) {
        return snapshot;
    }

    // Capture environment state
    snapshot.environment.current_time = env->current_time.get_seconds();
    snapshot.environment.time_of_next_psi = env->time_of_next_psi.get_seconds();
    snapshot.environment.time_of_last_psi = env->time_of_last_psi.get_seconds();
    snapshot.environment.next_movement_check = env->next_movement_check;
    snapshot.environment.current_time_code = env->current_time_code;

    // RNG seeds
    snapshot.environment.ivp_seed = ivp_srand_read();
    snapshot.environment.qh_seed = qh_srand_read();

    // CKIpionManager timing
    snapshot.environment.delta_time = ipionManager->GetDeltaTime();
    snapshot.environment.physics_delta_time = ipionManager->GetPhysicsDeltaTime();

    // Capture core states from m_MovableObjects (canonical ordering by index)
    auto &movableObjects = ipionManager->m_MovableObjects;
    std::unordered_set<IVP_Core *> seenCores;
    seenCores.reserve(movableObjects.n_elems);

    for (int i = 0; i < movableObjects.n_elems; ++i) {
        IVP_Real_Object *obj = movableObjects.element_at(i);
        if (!obj) continue;

        IVP_Core *core = obj->physical_core;
        if (!core) continue;

        // Deduplicate: merged sim units share a physical_core
        if (!seenCores.insert(core).second) continue;

        snapshot.cores.push_back(CaptureCoreState(core));
    }

    return snapshot;
}

// ============================================================================
// Layout Validation
// ============================================================================

bool ValidatePhysicsLayout(CKIpionManager *ipionManager) {
    if (!ipionManager) {
        Log::Error("ValidatePhysicsLayout: CKIpionManager is null");
        return false;
    }

    IVP_Environment *env = ipionManager->GetEnvironment();
    if (!env) {
        Log::Error("ValidatePhysicsLayout: IVP_Environment is null");
        return false;
    }

    // Check delta_PSI_time ~= 1.0/66.0 (~0.015151)
    double deltaPSI = env->delta_PSI_time;
    double expected = 1.0 / 66.0;
    if (std::abs(deltaPSI - expected) > 0.001) {
        Log::Error("ValidatePhysicsLayout: delta_PSI_time = %f, expected ~%f", deltaPSI, expected);
        return false;
    }

    // Check environment_magic_number == 123456
    if (env->environment_magic_number != IVP_Environment_Magic_Number) {
        Log::Error("ValidatePhysicsLayout: magic_number = %d, expected %d",
                   env->environment_magic_number, IVP_Environment_Magic_Number);
        return false;
    }

    // Check m_PhysicsTimeFactor == 0.001f (Ballance default)
    float timeFactor = ipionManager->GetPhysicsTimeFactor();
    if (std::abs(timeFactor - 0.001f) > 0.0001f) {
        Log::Error("ValidatePhysicsLayout: PhysicsTimeFactor = %f, expected 0.001", timeFactor);
        return false;
    }

    Log::Info("ValidatePhysicsLayout: All checks passed "
              "(delta_PSI=%.6f, magic=%d, timeFactor=%.4f)",
              deltaPSI, env->environment_magic_number, timeFactor);
    return true;
}

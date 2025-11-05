#include "LuaApi.h"

#include <stdexcept>

#include "TASEngine.h"
#include "GameInterface.h"
#include "ScriptContext.h"

// ===================================================================
//  World Query API Registration
// ===================================================================

void LuaApi::RegisterWorldQueryApi(sol::table &tas, ScriptContext *context) {
    if (!context) {
        throw std::runtime_error("LuaApi::RegisterWorldQueryApi requires a valid ScriptContext");
    }

    // tas.is_paused()
    tas["is_paused"] = [context]() -> bool {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsPaused();
    };

    // tas.is_playing()
    tas["is_playing"] = [context]() -> bool {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return false;
        }

        return g->IsPlaying();
    };

    // tas.get_sr_score()
    tas["get_sr_score"] = [context]() -> float {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return 0.0f;
        }

        return g->GetSRScore();
    };

    // tas.get_hs_score()
    tas["get_hs_score"] = [context]() -> int {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetHSScore();
    };

    // tas.get_points()
    tas["get_points"] = [context]() -> int {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetPoints();
    };

    // tas.get_life_count()
    tas["get_life_count"] = [context]() -> int {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetLifeCount();
    };

    // tas.get_level()
    tas["get_level"] = [context]() -> int {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetCurrentLevel();
    };

    // tas.get_sector()
    tas["get_sector"] = [context]() -> int {
        const auto *g = context->GetGameInterface();
        if (!g) {
            return 0;
        }

        return g->GetCurrentSector();
    };

    // tas.get_object(name)
    tas["get_object"] = [context](const std::string &name) -> sol::object {
        if (name.empty()) {
            return sol::nil;
        }
        try {
            const auto *g = context->GetGameInterface();
            if (g) {
                CK3dEntity *obj = g->GetObjectByName(name);
                if (obj) {
                    return sol::make_object(context->GetLuaState(), obj);
                }
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_object_by_id(id)
    tas["get_object_by_id"] = [context](int id) -> sol::object {
        if (id <= 0) {
            return sol::nil;
        }
        try {
            const auto *g = context->GetGameInterface();
            if (!g) {
                return sol::nil;
            }
            CK3dEntity *obj = g->GetObjectByID(id);
            if (obj) {
                return sol::make_object(context->GetLuaState(), obj);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_physics_object(entity)
    tas["get_physics_object"] = [context](CK3dEntity *entity) -> sol::object {
        if (!entity) {
            return sol::nil;
        }
        try {
            const auto *g = context->GetGameInterface();
            if (!g) {
                return sol::nil;
            }
            PhysicsObject *obj = g->GetPhysicsObject(entity);
            if (obj) {
                return sol::make_object(context->GetLuaState(), obj);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball()
    tas["get_ball"] = [context]() -> sol::object {
        try {
            const auto *g = context->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(context->GetLuaState(), ball);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_position()
    tas["get_ball_position"] = [context]() -> sol::object {
        try {
            const auto *g = context->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(context->GetLuaState(), g->GetPosition(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_velocity()
    tas["get_ball_velocity"] = [context]() -> sol::object {
        try {
            const auto *g = context->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(context->GetLuaState(), g->GetVelocity(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_ball_angular_velocity()
    tas["get_ball_angular_velocity"] = [context]() -> sol::object {
        try {
            const auto *g = context->GetGameInterface();
            CK3dEntity *ball = g->GetActiveBall();
            if (ball) {
                return sol::make_object(context->GetLuaState(), g->GetAngularVelocity(ball));
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // tas.get_camera()
    tas["get_camera"] = [context]() -> sol::object {
        try {
            const auto *g = context->GetGameInterface();
            CK3dEntity *camera = g->GetActiveCamera();
            if (camera) {
                return sol::make_object(context->GetLuaState(), camera);
            }
        } catch (const std::exception &) {
            // Fall through to return nil
        }
        return sol::nil;
    };

    // ===================================================================
    // RNG State Management API (tas.rng.*)
    // ===================================================================

    sol::table rng = tas["rng"] = tas.create();

    // tas.rng.get_state() - Get current RNG state
    rng["get_state"] = [context]() -> sol::object {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                return sol::nil;
            }
            RNGState state = g->GetRNGState();
            auto &lua = context->GetLuaState();
            sol::table result = lua.create_table();
            result["id"] = state.id;
            result["next_movement_check"] = state.next_movement_check;
            result["ivp_seed"] = state.ivp_seed;
            result["qh_seed"] = state.qh_seed;
            return result;
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.get_state: ") + e.what());
        }
    };

    // tas.rng.push_state() - Save current RNG state to stack
    rng["push_state"] = [context]() {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                throw sol::error("rng.push_state: GameInterface not available");
            }
            g->PushRNGState();
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.push_state: ") + e.what());
        }
    };

    // tas.rng.pop_state() - Restore RNG state from stack
    rng["pop_state"] = [context]() {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                throw sol::error("rng.pop_state: GameInterface not available");
            }
            g->PopRNGState();
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.pop_state: ") + e.what());
        }
    };

    // tas.rng.clear_stack() - Clear all saved RNG states
    rng["clear_stack"] = [context]() {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                throw sol::error("rng.clear_stack: GameInterface not available");
            }
            g->ClearRNGStateStack();
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.clear_stack: ") + e.what());
        }
    };

    // tas.rng.get_stack_depth() - Get the depth of RNG state stack
    rng["get_stack_depth"] = [context]() -> size_t {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                return 0;
            }
            return g->GetRNGStateStackDepth();
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.get_stack_depth: ") + e.what());
        }
    };

    // tas.rng.is_stack_empty() - Check if RNG state stack is empty
    rng["is_stack_empty"] = [context]() -> bool {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                return true;
            }
            return g->IsRNGStateStackEmpty();
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.is_stack_empty: ") + e.what());
        }
    };

    // tas.rng.reset_id() - Reset RNG state ID counter to 1
    rng["reset_id"] = [context]() {
        try {
            auto *g = context->GetGameInterface();
            if (!g) {
                throw sol::error("rng.reset_id: GameInterface not available");
            }
            g->ResetRNGStateID();
        } catch (const std::exception &e) {
            throw sol::error(std::string("rng.reset_id: ") + e.what());
        }
    };
}

#include "LuaApi.h"

#include <CKLevel.h>
#include <CKScene.h>
#include <CKPlace.h>
#include <CKRenderContext.h>

void LuaApi::RegisterCKLevel(sol::state &lua) {
    // ===================================================================
    //  CKLevel - Main composition management
    // ===================================================================
    auto ckLevelType = lua.new_usertype<CKLevel>(
        "CKLevel",
        sol::no_constructor, // Objects are created through CKContext, not directly

        // Object management
        "add_object", &CKLevel::AddObject,
        "remove_object", sol::overload(
            static_cast<CKERROR (CKLevel::*)(CKObject*)>(&CKLevel::RemoveObject),
            static_cast<CKERROR (CKLevel::*)(CK_ID)>(&CKLevel::RemoveObject)
        ),
        "begin_add_sequence", &CKLevel::BeginAddSequence,
        "begin_remove_sequence", &CKLevel::BeginRemoveSequence,

        // Object list computation
        "compute_object_list", &CKLevel::ComputeObjectList,

        // Place management
        "add_place", &CKLevel::AddPlace,
        "remove_place", sol::overload(
            static_cast<CKERROR (CKLevel::*)(CKPlace*)>(&CKLevel::RemovePlace),
            static_cast<CKPlace* (CKLevel::*)(int)>(&CKLevel::RemovePlace)
        ),
        "get_place", &CKLevel::GetPlace,
        "place_count", sol::property(&CKLevel::GetPlaceCount),

        // Scene management
        "add_scene", &CKLevel::AddScene,
        "remove_scene", sol::overload(
            static_cast<CKERROR (CKLevel::*)(CKScene*)>(&CKLevel::RemoveScene),
            static_cast<CKScene* (CKLevel::*)(int)>(&CKLevel::RemoveScene)
        ),
        "get_scene", &CKLevel::GetScene,
        "scene_count", sol::property(&CKLevel::GetSceneCount),

        // Active scene management
        "set_next_active_scene", sol::overload(
            [](CKLevel &level, CKScene *scene) {
                return level.SetNextActiveScene(scene);
            },
            [](CKLevel &level, CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS active) {
                return level.SetNextActiveScene(scene, active);
            },
            [](CKLevel &level, CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS active, CK_SCENEOBJECTRESET_FLAGS reset) {
                return level.SetNextActiveScene(scene, active, reset);
            }
        ),
        "launch_scene", sol::overload(
            [](CKLevel &level, CKScene *scene) {
                return level.LaunchScene(scene);
            },
            [](CKLevel &level, CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS active) {
                return level.LaunchScene(scene, active);
            },
            [](CKLevel &level, CKScene *scene, CK_SCENEOBJECTACTIVITY_FLAGS active, CK_SCENEOBJECTRESET_FLAGS reset) {
                return level.LaunchScene(scene, active, reset);
            }
        ),
        "get_current_scene", &CKLevel::GetCurrentScene,

        // Render context management
        // "add_render_context", sol::overload(
        //     [](CKLevel &level, CKRenderContext *ctx) {
        //         level.AddRenderContext(ctx);
        //     },
        //     [](CKLevel &level, CKRenderContext *ctx, bool main) {
        //         level.AddRenderContext(ctx, main);
        //     }
        // ),
        // "remove_render_context", &CKLevel::RemoveRenderContext,
        // "get_render_context", &CKLevel::GetRenderContext,
        // "render_context_count", sol::property(&CKLevel::GetRenderContextCount),

        // Level scene
        "get_level_scene", &CKLevel::GetLevelScene,

        // Merge functionality
        "merge", &CKLevel::Merge,

        // Static cast method
        "cast", &CKLevel::Cast
    );
}
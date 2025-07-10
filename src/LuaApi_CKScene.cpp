#include "LuaApi.h"

#include <CKScene.h>
#include <CKSceneObject.h>
#include <CKCamera.h>
#include <CKTexture.h>
#include <CKLevel.h>
#include <CKStateChunk.h>

void LuaApi::RegisterCKScene(sol::state &lua) {
    // ===================================================================
    //  CKScene - Narrative management
    // ===================================================================
    auto ckSceneType = lua.new_usertype<CKScene>(
        "CKScene",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKBeObject, CKSceneObject, CKObject>(),

        // Object functions
        "add_object_to_scene", sol::overload(
            [](CKScene &scene, CKSceneObject *obj) { scene.AddObjectToScene(obj); },
            [](CKScene &scene, CKSceneObject *obj, CKBOOL deps) { scene.AddObjectToScene(obj, deps); }
        ),
        "remove_object_from_scene", sol::overload(
            [](CKScene &scene, CKSceneObject *obj) { scene.RemoveObjectFromScene(obj); },
            [](CKScene &scene, CKSceneObject *obj, CKBOOL deps) { scene.RemoveObjectFromScene(obj, deps); }
        ),
        "is_object_here", &CKScene::IsObjectHere,
        "begin_add_sequence", &CKScene::BeginAddSequence,
        "begin_remove_sequence", &CKScene::BeginRemoveSequence,

        // Object list
        "get_object_count", &CKScene::GetObjectCount,
        "compute_object_list", sol::overload(
            [](CKScene &scene, CK_CLASSID cid) { return scene.ComputeObjectList(cid); },
            [](CKScene &scene, CK_CLASSID cid, CKBOOL derived) { return scene.ComputeObjectList(cid, derived); }
        ),
        "get_object_iterator", &CKScene::GetObjectIterator,

        // Object activation
        "activate", &CKScene::Activate,
        "deactivate", &CKScene::DeActivate,

        // Object settings
        "set_object_flags", &CKScene::SetObjectFlags,
        "get_object_flags", &CKScene::GetObjectFlags,
        "modify_object_flags", &CKScene::ModifyObjectFlags,
        "set_object_initial_value", &CKScene::SetObjectInitialValue,
        "get_object_initial_value", &CKScene::GetObjectInitialValue,
        "is_object_active", &CKScene::IsObjectActive,

        // Render settings
        // "apply_environment_settings", sol::overload(
        //     [](CKScene &scene) { scene.ApplyEnvironmentSettings(); },
        //     [](CKScene &scene, XObjectPointerArray *list) { scene.ApplyEnvironmentSettings(list); }
        // ),
        // "use_environment_settings", sol::overload(
        //     [](CKScene &scene) { scene.UseEnvironmentSettings(); },
        //     [](CKScene &scene, CKBOOL use) { scene.UseEnvironmentSettings(use); }
        // ),
        // "environment_settings", &CKScene::EnvironmentSettings,

        // Ambient light
        "set_ambient_light", &CKScene::SetAmbientLight,
        "get_ambient_light", &CKScene::GetAmbientLight,

        // Fog access
        "set_fog_mode", &CKScene::SetFogMode,
        "set_fog_start", &CKScene::SetFogStart,
        "set_fog_end", &CKScene::SetFogEnd,
        "set_fog_density", &CKScene::SetFogDensity,
        "set_fog_color", &CKScene::SetFogColor,
        "get_fog_mode", &CKScene::GetFogMode,
        "get_fog_start", &CKScene::GetFogStart,
        "get_fog_end", &CKScene::GetFogEnd,
        "get_fog_density", &CKScene::GetFogDensity,
        "get_fog_color", &CKScene::GetFogColor,

        // Background
        "set_background_color", &CKScene::SetBackgroundColor,
        "get_background_color", &CKScene::GetBackgroundColor,
        "set_background_texture", &CKScene::SetBackgroundTexture,
        "get_background_texture", &CKScene::GetBackgroundTexture,

        // Active camera
        "set_starting_camera", &CKScene::SetStartingCamera,
        "get_starting_camera", &CKScene::GetStartingCamera,

        // Level functions
        "get_level", &CKScene::GetLevel,

        // Merge functions
        "merge", sol::overload(
            [](CKScene &scene, CKScene *merged) { return scene.Merge(merged); },
            [](CKScene &scene, CKScene *merged, CKLevel *from) { return scene.Merge(merged, from); }
        )
    );

    // ===================================================================
    //  CKSceneObjectIterator - Helper class for iterating scene objects
    // ===================================================================
    auto ckSceneObjectIteratorType = lua.new_usertype<CKSceneObjectIterator>(
        "CKSceneObjectIterator",
        sol::no_constructor,

        "get_object_id", &CKSceneObjectIterator::GetObjectID,
        "get_object_desc", &CKSceneObjectIterator::GetObjectDesc,
        "rewind", &CKSceneObjectIterator::Rewind,
        "remove_at", &CKSceneObjectIterator::RemoveAt,
        "end", &CKSceneObjectIterator::End,

        // Increment operator
        "next", [](CKSceneObjectIterator &it) {
            return it++;
        }
    );
}
#include "LuaApi.h"

#include <CKSceneObject.h>
#include <CKScene.h>

void LuaApi::RegisterCKSceneObject(sol::state &lua) {
    // ===================================================================
    //  CKSceneObject - Base class for objects which can be referenced in a scene
    // ===================================================================
    auto ckSceneObjectType = lua.new_usertype<CKSceneObject>(
        "CKSceneObject",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKObject>(),

        // Scene activity
        // "is_active_in_scene", [](CKSceneObject *obj, CKScene *scene) -> bool { return obj->IsActiveInScene(scene); },
        "is_active_in_current_scene", [](CKSceneObject *obj) -> bool { return obj->IsActiveInCurrentScene(); }

        // Scene presence
        // "is_in_scene", [](CKSceneObject *obj, CKScene *scene) -> bool { return obj->IsInScene(scene); },
        // "get_scene_in_count", &CKSceneObject::GetSceneInCount,
        // "get_scene_in", &CKSceneObject::GetSceneIn
    );
}

#include "LuaApi.h"

#include <CKTargetCamera.h>

void LuaApi::RegisterCKTargetCamera(sol::state &lua) {
    // ===================================================================
    //  CKTargetCamera - A camera with a target
    // ===================================================================
    auto ckTargetCameraType = lua.new_usertype<CKTargetCamera>(
        "CKTargetCamera",
        sol::no_constructor, // Objects are created through CKContext, not directly

        // CKTargetCamera inherits all functionality from CKCamera
        // The main difference is that it always points to its target 3D entity
        // All CKCamera methods are inherited automatically

        // Static cast method
        "cast", &CKTargetCamera::Cast,

        // Inherit from CKCamera
        sol::base_classes, sol::bases<CKCamera>()
    );
}
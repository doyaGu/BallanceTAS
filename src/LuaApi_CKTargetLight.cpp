#include "LuaApi.h"

#include <CKTargetLight.h>
#include <CKLight.h>

void LuaApi::RegisterCKTargetLight(sol::state &lua) {
    // ===================================================================
    //  CKTargetLight - A light with a target
    // ===================================================================
    auto ckTargetLightType = lua.new_usertype<CKTargetLight>(
        "CKTargetLight",
        sol::no_constructor, // Objects are created through CKContext, not directly

        // CKTargetLight inherits all functionality from CKLight
        // The main difference is that it always points to its target 3D entity
        // All CKLight methods are inherited automatically

        // Static cast method
        "cast", &CKTargetLight::Cast,

        // Inherit from CKLight
        sol::base_classes, sol::bases<CKLight>()
    );
}
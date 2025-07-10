#include "LuaApi.h"

#include <CKLight.h>
#include <CK3dEntity.h>
#include <VxColor.h>

void LuaApi::RegisterCKLight(sol::state &lua) {
    // ===================================================================
    //  CKLight - Class for lights (spot, directional, point)
    // ===================================================================
    auto ckLightType = lua.new_usertype<CKLight>(
        "CKLight",
        sol::no_constructor,
        sol::base_classes, sol::bases<CK3dEntity, CKRenderObject, CKBeObject, CKSceneObject, CKObject>(),

        // Color management
        "set_color", &CKLight::SetColor,
        "get_color", &CKLight::GetColor,

        // Attenuation
        "set_constant_attenuation", &CKLight::SetConstantAttenuation,
        "set_linear_attenuation", &CKLight::SetLinearAttenuation,
        "set_quadratic_attenuation", &CKLight::SetQuadraticAttenuation,
        "get_constant_attenuation", &CKLight::GetConstantAttenuation,
        "get_linear_attenuation", &CKLight::GetLinearAttenuation,
        "get_quadratic_attenuation", &CKLight::GetQuadraticAttenuation,

        // Type
        "get_type", &CKLight::GetType,
        "set_type", &CKLight::SetType,

        // Range
        "get_range", &CKLight::GetRange,
        "set_range", &CKLight::SetRange,

        // Spotlight options
        "get_hot_spot", &CKLight::GetHotSpot,
        "get_fall_off", &CKLight::GetFallOff,
        "set_hot_spot", &CKLight::SetHotSpot,
        "set_fall_off", &CKLight::SetFallOff,
        "get_fall_off_shape", &CKLight::GetFallOffShape,
        "set_fall_off_shape", &CKLight::SetFallOffShape,

        // Activity
        "active", &CKLight::Active,
        "get_activity", &CKLight::GetActivity,
        "set_specular_flag", &CKLight::SetSpecularFlag,
        "get_specular_flag", &CKLight::GetSpecularFlag,

        // Target access
        "get_target", &CKLight::GetTarget,
        "set_target", &CKLight::SetTarget,

        // Light power
        "get_light_power", &CKLight::GetLightPower,
        "set_light_power", sol::overload(
            [](CKLight &light) { light.SetLightPower(); },
            [](CKLight &light, float power) { light.SetLightPower(power); }
        )
    );
}
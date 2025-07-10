#include "LuaApi.h"

#include <CKMaterial.h>
#include <CKTexture.h>
#include <CKRenderContext.h>
#include <VxColor.h>

void LuaApi::RegisterCKMaterial(sol::state &lua) {
    // ===================================================================
    //  CKMaterial - Color and texture settings for objects
    // ===================================================================
    auto ckMaterialType = lua.new_usertype<CKMaterial>(
        "CKMaterial",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKBeObject, CKSceneObject, CKObject>(),

        // Specular power
        "get_power", &CKMaterial::GetPower,
        "set_power", &CKMaterial::SetPower,

        // Color properties
        "get_ambient", &CKMaterial::GetAmbient,
        "set_ambient", &CKMaterial::SetAmbient,
        "get_diffuse", &CKMaterial::GetDiffuse,
        "set_diffuse", &CKMaterial::SetDiffuse,
        "get_specular", &CKMaterial::GetSpecular,
        "set_specular", &CKMaterial::SetSpecular,
        "get_emissive", &CKMaterial::GetEmissive,
        "set_emissive", &CKMaterial::SetEmissive,

        // Texture
        "get_texture", sol::overload(
            [](CKMaterial &mat) { return mat.GetTexture(); },
            [](CKMaterial &mat, int index) { return mat.GetTexture(index); }
        ),
        "set_texture", sol::overload(&CKMaterial::SetTexture,  &CKMaterial::SetTexture0),

        // Texture blend mode
        "set_texture_blend_mode", &CKMaterial::SetTextureBlendMode,
        "get_texture_blend_mode", &CKMaterial::GetTextureBlendMode,

        // Texture filter mode
        "set_texture_min_mode", &CKMaterial::SetTextureMinMode,
        "get_texture_min_mode", &CKMaterial::GetTextureMinMode,
        "set_texture_mag_mode", &CKMaterial::SetTextureMagMode,
        "get_texture_mag_mode", &CKMaterial::GetTextureMagMode,

        // Texture address mode
        "set_texture_address_mode", &CKMaterial::SetTextureAddressMode,
        "get_texture_address_mode", &CKMaterial::GetTextureAddressMode,

        // Texture border color
        "set_texture_border_color", &CKMaterial::SetTextureBorderColor,
        "get_texture_border_color", &CKMaterial::GetTextureBorderColor,

        // Blend factors
        "set_source_blend", &CKMaterial::SetSourceBlend,
        "set_dest_blend", &CKMaterial::SetDestBlend,
        "get_source_blend", &CKMaterial::GetSourceBlend,
        "get_dest_blend", &CKMaterial::GetDestBlend,

        // Two sided material
        "is_two_sided", &CKMaterial::IsTwoSided,
        "set_two_sided", &CKMaterial::SetTwoSided,

        // Z buffer writing
        "z_write_enabled", &CKMaterial::ZWriteEnabled,
        "enable_z_write", sol::overload(
            [](CKMaterial &mat) { mat.EnableZWrite(); },
            [](CKMaterial &mat, CKBOOL enable) { mat.EnableZWrite(enable); }
        ),

        // Alpha blending
        "alpha_blend_enabled", &CKMaterial::AlphaBlendEnabled,
        "enable_alpha_blend", sol::overload(
            [](CKMaterial &mat) { mat.EnableAlphaBlend(); },
            [](CKMaterial &mat, CKBOOL enable) { mat.EnableAlphaBlend(enable); }
        ),

        // Z comparison function
        "get_z_func", &CKMaterial::GetZFunc,
        "set_z_func", sol::overload(
            [](CKMaterial &mat) { mat.SetZFunc(); },
            [](CKMaterial &mat, VXCMPFUNC func) { mat.SetZFunc(func); }
        ),

        // Perspective correction
        "perspective_correction_enabled", &CKMaterial::PerspectiveCorrectionEnabled,
        "enable_perspective_correction", sol::overload(
            [](CKMaterial &mat) { mat.EnablePerspectiveCorrection(); },
            [](CKMaterial &mat, CKBOOL enable) { mat.EnablePerspectiveCorrection(enable); }
        ),

        // Fill mode
        "set_fill_mode", &CKMaterial::SetFillMode,
        "get_fill_mode", &CKMaterial::GetFillMode,

        // Shade mode
        "set_shade_mode", &CKMaterial::SetShadeMode,
        "get_shade_mode", &CKMaterial::GetShadeMode,

        // Current material
        "set_as_current", sol::overload(
            [](CKMaterial &mat, CKRenderContext *ctx) { return mat.SetAsCurrent(ctx); },
            [](CKMaterial &mat, CKRenderContext *ctx, CKBOOL lit) { return mat.SetAsCurrent(ctx, lit); },
            [](CKMaterial &mat, CKRenderContext *ctx, CKBOOL lit, int stage) { return mat.SetAsCurrent(ctx, lit, stage); }
        ),

        // Transparency check
        "is_alpha_transparent", &CKMaterial::IsAlphaTransparent,

        // Alpha testing
        "alpha_test_enabled", &CKMaterial::AlphaTestEnabled,
        "enable_alpha_test", sol::overload(
            [](CKMaterial &mat) { mat.EnableAlphaTest(); },
            [](CKMaterial &mat, CKBOOL enable) { mat.EnableAlphaTest(enable); }
        ),
        "get_alpha_func", &CKMaterial::GetAlphaFunc,
        "set_alpha_func", sol::overload(
            [](CKMaterial &mat) { mat.SetAlphaFunc(); },
            [](CKMaterial &mat, VXCMPFUNC func) { mat.SetAlphaFunc(func); }
        ),

        // Alpha reference value
        "get_alpha_ref", &CKMaterial::GetAlphaRef,
        "set_alpha_ref", sol::overload(
            [](CKMaterial &mat) { mat.SetAlphaRef(); },
            [](CKMaterial &mat, CKBYTE ref) { mat.SetAlphaRef(ref); }
        )

        // Effects
        // "set_effect", &CKMaterial::SetEffect,
        // "get_effect", &CKMaterial::GetEffect,
        // "get_effect_parameter", &CKMaterial::GetEffectParameter,

        // Callback - Note: Complex callback handling might need special implementation
        // "set_callback", [](CKMaterial &mat) {
        //     throw sol::error("Material callbacks from Lua not yet implemented");
        // },
        // "get_callback", [](CKMaterial &mat) {
        //     throw sol::error("Material callbacks from Lua not yet implemented");
        // }
    );
}
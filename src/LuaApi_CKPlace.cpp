#include "LuaApi.h"

#include <CKPlace.h>
#include <CKCamera.h>
#include <CK3dEntity.h>
#include <VxRect.h>
#include <VxMatrix.h>

void LuaApi::RegisterCKPlace(sol::state &lua) {
    // ===================================================================
    //  CKPlace - Group of 3D entities
    // ===================================================================
    auto ckPlaceType = lua.new_usertype<CKPlace>(
        "CKPlace",
        sol::no_constructor,
        sol::base_classes, sol::bases<CK3dEntity, CKRenderObject, CKBeObject, CKSceneObject, CKObject>(),

        // Camera
        "get_default_camera", &CKPlace::GetDefaultCamera,
        "set_default_camera", &CKPlace::SetDefaultCamera,

        // Portals
        "add_portal", &CKPlace::AddPortal,
        "remove_portal", &CKPlace::RemovePortal,
        "get_portal_count", &CKPlace::GetPortalCount,
        "get_portal", &CKPlace::GetPortal,
        "viewport_clip", &CKPlace::ViewportClip,
        "compute_best_fit_bbox", &CKPlace::ComputeBestFitBBox
    );
}
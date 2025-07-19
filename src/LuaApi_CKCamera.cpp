#include "LuaApi.h"

#include <CKCamera.h>
#include <VxMatrix.h>

void LuaApi::RegisterCKCamera(sol::state &lua) {
    // ===================================================================
    //  CKCamera - Camera object for 3D rendering
    // ===================================================================
    auto ckCameraType = lua.new_usertype<CKCamera>(
        "CKCamera",
        sol::no_constructor, // Objects are created through CKContext, not directly
        sol::base_classes, sol::bases<CK3dEntity, CKRenderObject, CKBeObject, CKSceneObject, CKObject>(),

        // Clipping planes
        "front_plane", sol::property(&CKCamera::GetFrontPlane, &CKCamera::SetFrontPlane),
        "back_plane", sol::property(&CKCamera::GetBackPlane, &CKCamera::SetBackPlane),

        // Field of view
        "fov", sol::property(&CKCamera::GetFov, &CKCamera::SetFov),

        // Projection type
        "projection_type", sol::property(&CKCamera::GetProjectionType, &CKCamera::SetProjectionType),

        // Orthographic zoom
        "orthographic_zoom", sol::property(&CKCamera::GetOrthographicZoom, &CKCamera::SetOrthographicZoom),

        // Aspect ratio
        "set_aspect_ratio", &CKCamera::SetAspectRatio,
        "get_aspect_ratio", [](CKCamera *camera) {
            int width, height;
            camera->GetAspectRatio(width, height);
            return Vx2DVector(width, height);
        },

        // Projection matrix computation
        "compute_projection_matrix", [](CKCamera *camera) {
            VxMatrix mat;
            camera->ComputeProjectionMatrix(mat);
            return mat;
        },

        // Roll operations
        "reset_roll", &CKCamera::ResetRoll,
        "roll", &CKCamera::Roll,

        // Target operations (may return NULL for non-target cameras)
        "get_target", &CKCamera::GetTarget,
        "set_target", &CKCamera::SetTarget
    );
}

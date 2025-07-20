//--- START OF FILE LuaApi_VxIntersectionDesc.cpp ---

#include "LuaApi.h"

#include <CKTypes.h>
#include <CKRenderObject.h>
#include <VxVector.h>

void LuaApi::RegisterVxIntersectionDesc(sol::state &lua) {
    // ===================================================================
    //  VxIntersectionDesc - Description of a ray intersection
    // ===================================================================
    auto intersectionDescType = lua.new_usertype<VxIntersectionDesc>(
        "VxIntersectionDesc",
        sol::no_constructor, // No direct construction

        // The object that was intersected. Read-only from Lua.
        "object", sol::readonly_property(&VxIntersectionDesc::Object),

        // The point of intersection in the object's local coordinates.
        "point", sol::readonly_property(&VxIntersectionDesc::IntersectionPoint),

        // The normal of the face at the intersection point.
        "normal", sol::readonly_property(&VxIntersectionDesc::IntersectionNormal),

        // Texture coordinates at the intersection point.
        "u", sol::readonly_property(&VxIntersectionDesc::TexU),
        "v", sol::readonly_property(&VxIntersectionDesc::TexV),

        // The distance from the ray's origin to the intersection point.
        "distance", sol::readonly_property(&VxIntersectionDesc::Distance),

        // The index of the face that was intersected.
        "face_index", sol::readonly_property(&VxIntersectionDesc::FaceIndex),

        // A convenient to_string metamethod for printing in Lua
        sol::meta_function::to_string, [](const VxIntersectionDesc &desc) {
            std::string objName = "nil";
            if (desc.Object) {
                // Check if the object has a name, otherwise use its ID.
                if (desc.Object->GetName()) {
                    objName = std::string("'") + desc.Object->GetName() + "'";
                } else {
                    objName = "ID: " + std::to_string(desc.Object->GetID());
                }
            }
            return "VxIntersectionDesc(obj: " + objName +
                   ", dist: " + std::to_string(desc.Distance) +
                   ", face: " + std::to_string(desc.FaceIndex) + ")";
        }
    );
}
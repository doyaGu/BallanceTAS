#include "LuaApi.h"

#include <CKRenderObject.h>

void LuaApi::RegisterCKRenderObject(sol::state &lua) {
    // ===================================================================
    //  CKRenderObject - Base class for objects that can be rendered
    // ===================================================================
    auto ckRenderObjectType = lua.new_usertype<CKRenderObject>(
        "CKRenderObject",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKBeObject, CKSceneObject, CKObject>(),

        // Render context queries
        // "is_in_render_context", &CKRenderObject::IsInRenderContext,
        "is_root_object", &CKRenderObject::IsRootObject,
        "is_to_be_rendered", &CKRenderObject::IsToBeRendered,
        "is_to_be_rendered_last", &CKRenderObject::IsToBeRenderedLast,

        // Z order
        "z_order", sol::property(&CKRenderObject::GetZOrder, &CKRenderObject::SetZOrder)

        // Render callbacks - Note: These might need special handling due to function pointers
        // "add_pre_render_callback", [](CKRenderObject &obj, sol::function func, bool temp = false) {
        //     // This would need a wrapper to convert sol::function to CK_RENDEROBJECT_CALLBACK
        //     // Implementation depends on how you want to handle Lua callbacks
        //     throw sol::error("Render callbacks from Lua not yet implemented");
        // },
        // "remove_pre_render_callback", [](CKRenderObject &obj) {
        //     throw sol::error("Render callbacks from Lua not yet implemented");
        // },
        // "set_render_callback", [](CKRenderObject &obj, sol::function func) {
        //     throw sol::error("Render callbacks from Lua not yet implemented");
        // },
        // "remove_render_callback", &CKRenderObject::RemoveRenderCallBack,
        // "add_post_render_callback", [](CKRenderObject &obj, sol::function func, bool temp = false) {
        //     throw sol::error("Render callbacks from Lua not yet implemented");
        // },
        // "remove_post_render_callback", [](CKRenderObject &obj) {
        //     throw sol::error("Render callbacks from Lua not yet implemented");
        // },
        // "remove_all_callbacks", &CKRenderObject::RemoveAllCallbacks
    );
}

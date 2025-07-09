#include "LuaApi.h"

#include <CKObject.h>

void LuaApi::RegisterCKObject(sol::state &lua) {
    // ===================================================================
    //  CKObject - Base class for most CK objects
    // ===================================================================
    auto ckObjectType = lua.new_usertype<CKObject>(
        "CKObject",
        sol::no_constructor, // Objects are created through CKContext, not directly

        // Basic properties
        "name", sol::property(&CKObject::GetName, &CKObject::SetName),
        "id", sol::property(&CKObject::GetID),
        "class_id", sol::property(&CKObject::GetClassID),
        "object_flags", sol::property(&CKObject::GetObjectFlags),

        // Status checks
        "is_dynamic", &CKObject::IsDynamic,
        "is_to_be_deleted", &CKObject::IsToBeDeleted,
        "is_visible", &CKObject::IsVisible,
        "is_hierarchically_hidden", &CKObject::IsHierarchicallyHide,
        "is_up_to_date", &CKObject::IsUpToDate,
        "is_private", &CKObject::IsPrivate,
        "is_not_to_be_saved", &CKObject::IsNotToBeSaved,
        "is_interface_obj", &CKObject::IsInterfaceObj,

        // Visibility control
        "show", sol::overload(
            [](CKObject &obj) { obj.Show(); },
            [](CKObject &obj, CK_OBJECT_SHOWOPTION option) { obj.Show(option); }
        ),
        "is_hidden_by_parent", &CKObject::IsHiddenByParent,
        "can_be_hide", &CKObject::CanBeHide,

        // App data
        // "get_app_data", &CKObject::GetAppData,
        // "set_app_data", &CKObject::SetAppData,

        // Context access
        "get_context", &CKObject::GetCKContext,

        // Object manipulation
        "modify_object_flags", &CKObject::ModifyObjectFlags
    );
}

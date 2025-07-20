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
        "id", sol::readonly_property(&CKObject::GetID),
        "class_id", sol::readonly_property(&CKObject::GetClassID),
        "object_flags", sol::readonly_property(&CKObject::GetObjectFlags),

        // Status checks
        "is_dynamic", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsDynamic(); }),
        "is_to_be_deleted", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsToBeDeleted(); }),
        "is_visible", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsVisible(); }),
        "is_hierarchically_hidden", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsHierarchicallyHide(); }),
        "is_up_to_date", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsUpToDate(); }),
        "is_private", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsPrivate(); }),
        "is_not_to_be_saved", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsNotToBeSaved(); }),
        "is_interface_obj", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsInterfaceObj(); }),

        // Visibility control
        "show", sol::overload(
            [](CKObject *obj) { obj->Show(); },
            [](CKObject *obj, CK_OBJECT_SHOWOPTION option) { obj->Show(option); }
        ),
        "is_hidden_by_parent", sol::readonly_property([](CKObject *obj) -> bool { return obj->IsHiddenByParent(); }),
        "can_be_hide", sol::readonly_property([](CKObject *obj) -> bool { return obj->CanBeHide(); }),

        // App data
        // "get_app_data", &CKObject::GetAppData,
        // "set_app_data", &CKObject::SetAppData,

        // Context access
        // "context", sol::readonly_property(&CKObject::GetCKContext),

        // Object manipulation
        "modify_object_flags", &CKObject::ModifyObjectFlags
    );
}

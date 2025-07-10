#include "LuaApi.h"

#include <CKGroup.h>
#include <CKBeObject.h>

void LuaApi::RegisterCKGroup(sol::state &lua) {
    // ===================================================================
    //  CKGroup - Management of group of objects
    // ===================================================================
    auto ckGroupType = lua.new_usertype<CKGroup>(
        "CKGroup",
        sol::no_constructor, // Objects are created through CKContext, not directly

        // Object insertion/removal
        "add_object", &CKGroup::AddObject,
        "add_object_front", &CKGroup::AddObjectFront,
        "insert_object_at", &CKGroup::InsertObjectAt,
        "remove_object", sol::overload(
            static_cast<CKBeObject* (CKGroup::*)(int)>(&CKGroup::RemoveObject),
            static_cast<void (CKGroup::*)(CKBeObject*)>(&CKGroup::RemoveObject)
        ),
        "clear", &CKGroup::Clear,

        // Object ordering
        "move_object_up", &CKGroup::MoveObjectUp,
        "move_object_down", &CKGroup::MoveObjectDown,

        // Object access
        "get_object", &CKGroup::GetObject,
        "object_count", sol::property(&CKGroup::GetObjectCount),

        // Common class ID
        "get_common_class_id", &CKGroup::GetCommonClassID,

        // Visibility
        "can_be_hide", &CKGroup::CanBeHide,
        "show", sol::overload(
            [](CKGroup &group) { group.Show(); },
            [](CKGroup &group, CK_OBJECT_SHOWOPTION option) { group.Show(option); }
        ),

        // Static cast method
        "cast", &CKGroup::Cast
    );
}
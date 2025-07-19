#include "LuaApi.h"

#include <CKBeObject.h>
#include <CKBehavior.h>
#include <CKParameterOut.h>
#include <CKMessage.h>

void LuaApi::RegisterCKBeObject(sol::state &lua) {
    // ===================================================================
    //  CKBeObject - Base class for objects with behaviors
    // ===================================================================
    auto ckBeObjectType = lua.new_usertype<CKBeObject>(
        "CKBeObject",
        sol::no_constructor,
        sol::base_classes, sol::bases<CKSceneObject, CKObject>(),

        // Group functions
        // "is_in_group", [](CKBeObject *obj, CKGroup *group) -> bool { return obj->IsInGroup(group); },

        // Attribute functions
        // "has_attribute", &CKBeObject::HasAttribute,
        // "set_attribute", sol::overload(
        //     [](CKBeObject *obj, CKAttributeType type) { return obj->SetAttribute(type); },
        //     [](CKBeObject *obj, CKAttributeType type, CK_ID param) { return obj->SetAttribute(type, param); }
        // ),
        // "remove_attribute", &CKBeObject::RemoveAttribute,
        // "get_attribute_parameter", &CKBeObject::GetAttributeParameter,
        // "get_attribute_count", &CKBeObject::GetAttributeCount,
        // "get_attribute_type", &CKBeObject::GetAttributeType,
        // "get_attribute_parameter_by_index", &CKBeObject::GetAttributeParameterByIndex,
        // "remove_all_attributes", &CKBeObject::RemoveAllAttributes,

        // Script functions
        // "add_script", &CKBeObject::AddScript,
        // "remove_script", sol::overload(
        //     [](CKBeObject *obj, CK_ID id) { return obj->RemoveScript(id); },
        //     [](CKBeObject *obj, int pos) { return obj->RemoveScript(pos); }
        // ),
        // "remove_all_scripts", &CKBeObject::RemoveAllScripts,
        // "get_script", &CKBeObject::GetScript,
        // "get_script_count", &CKBeObject::GetScriptCount,

        // Priority
        "get_priority", &CKBeObject::GetPriority,
        "set_priority", &CKBeObject::SetPriority,

        // Messages
        // "get_last_frame_message_count", &CKBeObject::GetLastFrameMessageCount,
        // "get_last_frame_message", &CKBeObject::GetLastFrameMessage,
        // "set_as_waiting_for_messages", sol::overload(
        //     [](CKBeObject *obj) { obj->SetAsWaitingForMessages(); },
        //     [](CKBeObject *obj, bool wait) { obj->SetAsWaitingForMessages(wait); }
        // ),
        // "is_waiting_for_messages", [](CKBeObject *obj) -> bool { return obj->IsWaitingForMessages(); },

        // Profiling
        "get_last_execution_time", &CKBeObject::GetLastExecutionTime
    );
}

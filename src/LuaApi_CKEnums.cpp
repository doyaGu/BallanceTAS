#include "LuaApi.h"

#include <CKDefines.h>
#include <CKObject.h>

void LuaApi::RegisterCKEnums(sol::state &lua) {
    // ===================================================================
    //  CK_OBJECT_FLAGS - General object flags
    // ===================================================================
    lua.new_enum<CK_OBJECT_FLAGS>(
        "CK_OBJECT_FLAGS", {
            // Basic object flags
            {"INTERFACEOBJ", CK_OBJECT_INTERFACEOBJ},
            {"PRIVATE", CK_OBJECT_PRIVATE},
            {"INTERFACEMARK", CK_OBJECT_INTERFACEMARK},
            {"FREEID", CK_OBJECT_FREEID},
            {"TOBEDELETED", CK_OBJECT_TOBEDELETED},
            {"NOTTOBESAVED", CK_OBJECT_NOTTOBESAVED},
            {"VISIBLE", CK_OBJECT_VISIBLE},
            {"NAMESHARED", CK_OBJECT_NAMESHARED},
            {"DYNAMIC", CK_OBJECT_DYNAMIC},
            {"HIERACHICALHIDE", CK_OBJECT_HIERACHICALHIDE},
            {"UPTODATE", CK_OBJECT_UPTODATE},
            {"TEMPMARKER", CK_OBJECT_TEMPMARKER},
            {"ONLYFORFILEREFERENCE", CK_OBJECT_ONLYFORFILEREFERENCE},
            {"NOTTOBEDELETED", CK_OBJECT_NOTTOBEDELETED},
            {"APPDATA", CK_OBJECT_APPDATA},
            {"SINGLEACTIVITY", CK_OBJECT_SINGLEACTIVITY},
            {"LOADSKIPBEOBJECT", CK_OBJECT_LOADSKIPBEOBJECT},

            // Combination flags
            {"NOTTOBELISTEDANDSAVED", CK_OBJECT_NOTTOBELISTEDANDSAVED},

            // Parameter-specific flags
            {"PARAMETEROUT_SETTINGS", CK_PARAMETEROUT_SETTINGS},
            {"PARAMETEROUT_PARAMOP", CK_PARAMETEROUT_PARAMOP},
            {"PARAMETERIN_DISABLED", CK_PARAMETERIN_DISABLED},
            {"PARAMETERIN_THIS", CK_PARAMETERIN_THIS},
            {"PARAMETERIN_SHARED", CK_PARAMETERIN_SHARED},
            {"PARAMETEROUT_DELETEAFTERUSE", CK_PARAMETEROUT_DELETEAFTERUSE},
            {"PARAMMASK", CK_OBJECT_PARAMMASK},

            // Behavior IO flags
            {"BEHAVIORIO_IN", CK_BEHAVIORIO_IN},
            {"BEHAVIORIO_OUT", CK_BEHAVIORIO_OUT},
            {"BEHAVIORIO_ACTIVE", CK_BEHAVIORIO_ACTIVE},
            {"IOTYPEMASK", CK_OBJECT_IOTYPEMASK},
            {"IOMASK", CK_OBJECT_IOMASK},

            // Behavior link flags
            {"BEHAVIORLINK_RESERVED", CKBEHAVIORLINK_RESERVED},
            {"BEHAVIORLINK_ACTIVATEDLASTFRAME", CKBEHAVIORLINK_ACTIVATEDLASTFRAME},
            {"BEHAVIORLINKMASK", CK_OBJECT_BEHAVIORLINKMASK}
        });

    // ===================================================================
    //  CK_OBJECT_SHOWOPTION - Options for showing/hiding objects
    // ===================================================================
    lua.new_enum<CK_OBJECT_SHOWOPTION>(
        "CK_OBJECT_SHOWOPTION",
        {
            {"CKHIDE", CKHIDE},
            {"CKSHOW", CKSHOW},
            {"CKHIERARCHICALHIDE", CKHIERARCHICALHIDE},
        }
    );

    // ===================================================================
    //  CK_3DENTITY_FLAGS - 3D Entity specific flags
    // ===================================================================
    lua.new_enum<CK_3DENTITY_FLAGS>(
        "CK_3DENTITY_FLAGS", {
            {"DUMMY", CK_3DENTITY_DUMMY},
            {"FRAME", CK_3DENTITY_FRAME},
            {"RESERVED0", CK_3DENTITY_RESERVED0},
            {"TARGETLIGHT", CK_3DENTITY_TARGETLIGHT},
            {"TARGETCAMERA", CK_3DENTITY_TARGETCAMERA},
            {"IGNOREANIMATION", CK_3DENTITY_IGNOREANIMATION},
            {"HIERARCHICALOBSTACLE", CK_3DENTITY_HIERARCHICALOBSTACLE},
            {"UPDATELASTFRAME", CK_3DENTITY_UPDATELASTFRAME},
            {"CAMERAIGNOREASPECT", CK_3DENTITY_CAMERAIGNOREASPECT},
            {"DISABLESKINPROCESS", CK_3DENTITY_DISABLESKINPROCESS},
            {"ENABLESKINOFFSET", CK_3DENTITY_ENABLESKINOFFSET},
            {"PLACEVALID", CK_3DENTITY_PLACEVALID},
            {"PARENTVALID", CK_3DENTITY_PARENTVALID},
            {"IKJOINTVALID", CK_3DENTITY_IKJOINTVALID},
            {"PORTAL", CK_3DENTITY_PORTAL},
            {"ZORDERVALID", CK_3DENTITY_ZORDERVALID},
            {"CHARACTERDOPROCESS", CK_3DENTITY_CHARACTERDOPROCESS}
        });

    // ===================================================================
    //  CK_RAYINTERSECTION - Ray intersection options
    // ===================================================================
    lua.new_enum<CK_RAYINTERSECTION>(
        "CK_RAYINTERSECTION", {
            {"DEFAULT", CKRAYINTERSECTION_DEFAULT},
            {"SEGMENT", CKRAYINTERSECTION_SEGMENT},
            {"IGNOREALPHA", CKRAYINTERSECTION_IGNOREALPHA},
            {"FIRSTCONTACT", CKRAYINTERSECTION_FIRSTCONTACT}
        });
}

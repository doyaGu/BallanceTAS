#pragma once

// Pushing and checking math values and game objects, shared by the bindings.

#include <VxMath.h>

#include "LuaRuntime/LuaHeaders.h"
#include "LuaRuntime/LuaUserdata.h"

class CK3dEntity;
class CKCamera;
class CKObject;
struct PhysicsObject;

namespace LuaApiCommon {
    // Metatable names of the userdata types.
    constexpr const char *kVxVectorMt = "BallanceTAS.VxVector";
    constexpr const char *kVx2DVectorMt = "BallanceTAS.Vx2DVector";
    constexpr const char *kVxVector4Mt = "BallanceTAS.VxVector4";
    constexpr const char *kVxBboxMt = "BallanceTAS.VxBbox";
    constexpr const char *kVxMatrixMt = "BallanceTAS.VxMatrix";
    constexpr const char *kVxQuaternionMt = "BallanceTAS.VxQuaternion";
    constexpr const char *kVxColorMt = "BallanceTAS.VxColor";
    constexpr const char *kVxRectMt = "BallanceTAS.VxRect";
    constexpr const char *kVxIntersectionDescMt = "BallanceTAS.VxIntersectionDesc";
    constexpr const char *kCKObjectMt = "BallanceTAS.CKObject";
    constexpr const char *kCKSceneObjectMt = "BallanceTAS.CKSceneObject";
    constexpr const char *kCKBeObjectMt = "BallanceTAS.CKBeObject";
    constexpr const char *kCKRenderObjectMt = "BallanceTAS.CKRenderObject";
    constexpr const char *kCK3dEntityMt = "BallanceTAS.CK3dEntity";
    constexpr const char *kCKCameraMt = "BallanceTAS.CKCamera";
    constexpr const char *kPhysicsObjectMt = "BallanceTAS.PhysicsObject";

    // Math values are pushed as owned userdata directly, so a script that reassigns the
    // global VxVector cannot break engine getters.
    void PushVxVector(lua_State *L, const VxVector &value);
    void PushVx2DVector(lua_State *L, const Vx2DVector &value);
    void PushVxVector4(lua_State *L, const VxVector4 &value);
    void PushVxMatrix(lua_State *L, const VxMatrix &value);
    void PushVxQuaternion(lua_State *L, const VxQuaternion &value);
    void PushVxBbox(lua_State *L, const VxBbox &value);

    inline VxVector *CheckVxVector(lua_State *L, int arg) {
        return tas::lua::CheckUserdata<VxVector>(L, arg, kVxVectorMt);
    }
    inline Vx2DVector *CheckVx2DVector(lua_State *L, int arg) {
        return tas::lua::CheckUserdata<Vx2DVector>(L, arg, kVx2DVectorMt);
    }
    inline VxVector4 *CheckVxVector4(lua_State *L, int arg) {
        return tas::lua::CheckUserdata<VxVector4>(L, arg, kVxVector4Mt);
    }
    inline VxMatrix *CheckVxMatrix(lua_State *L, int arg) {
        return tas::lua::CheckUserdata<VxMatrix>(L, arg, kVxMatrixMt);
    }
    inline VxQuaternion *CheckVxQuaternion(lua_State *L, int arg) {
        return tas::lua::CheckUserdata<VxQuaternion>(L, arg, kVxQuaternionMt);
    }

    // Game objects are borrowed; nullptr pushes nil.
    void PushCKObject(lua_State *L, CKObject *object);
    void PushCK3dEntity(lua_State *L, CK3dEntity *entity);
    void PushCKCamera(lua_State *L, CKCamera *camera);
    void PushPhysicsObject(lua_State *L, PhysicsObject *object);

    CK3dEntity *CheckCK3dEntity(lua_State *L, int arg);
    // nil/none gives nullptr.
    CK3dEntity *OptCK3dEntity(lua_State *L, int arg);

}

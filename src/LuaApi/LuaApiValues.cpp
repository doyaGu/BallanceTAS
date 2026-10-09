#include "LuaApiValues.h"

#include <CK3dEntity.h>
#include <CKCamera.h>
#include <CKObject.h>

#include "physics_RT.h"

namespace LuaApiCommon {
    void PushVxVector(lua_State *L, const VxVector &value) {
        tas::lua::PushOwnedUserdata<VxVector>(L, kVxVectorMt, value);
    }

    void PushVx2DVector(lua_State *L, const Vx2DVector &value) {
        tas::lua::PushOwnedUserdata<Vx2DVector>(L, kVx2DVectorMt, value);
    }

    void PushVxVector4(lua_State *L, const VxVector4 &value) {
        tas::lua::PushOwnedUserdata<VxVector4>(L, kVxVector4Mt, value);
    }

    void PushVxMatrix(lua_State *L, const VxMatrix &value) {
        tas::lua::PushOwnedUserdata<VxMatrix>(L, kVxMatrixMt, value);
    }

    void PushVxQuaternion(lua_State *L, const VxQuaternion &value) {
        tas::lua::PushOwnedUserdata<VxQuaternion>(L, kVxQuaternionMt, value);
    }

    void PushVxBbox(lua_State *L, const VxBbox &value) {
        tas::lua::PushOwnedUserdata<VxBbox>(L, kVxBboxMt, value);
    }

    void PushCKObject(lua_State *L, CKObject *object) {
        if (!object) {
            lua_pushnil(L);
            return;
        }
        tas::lua::PushBorrowedUserdata<CKObject>(L, kCKObjectMt, object);
    }

    void PushCK3dEntity(lua_State *L, CK3dEntity *entity) {
        if (!entity) {
            lua_pushnil(L);
            return;
        }
        tas::lua::PushBorrowedUserdata<CK3dEntity>(L, kCK3dEntityMt, entity);
    }

    void PushCKCamera(lua_State *L, CKCamera *camera) {
        if (!camera) {
            lua_pushnil(L);
            return;
        }
        tas::lua::PushBorrowedUserdata<CKCamera>(L, kCKCameraMt, camera);
    }

    void PushPhysicsObject(lua_State *L, PhysicsObject *object) {
        if (!object) {
            lua_pushnil(L);
            return;
        }
        tas::lua::PushBorrowedUserdata<PhysicsObject>(L, kPhysicsObjectMt, object);
    }

    CK3dEntity *CheckCK3dEntity(lua_State *L, int arg) {
        return tas::lua::CheckUserdata<CK3dEntity>(L, arg, kCK3dEntityMt);
    }

    CK3dEntity *OptCK3dEntity(lua_State *L, int arg) {
        return lua_isnoneornil(L, arg) ? nullptr : CheckCK3dEntity(L, arg);
    }

}

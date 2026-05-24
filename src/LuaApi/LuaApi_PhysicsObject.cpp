#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include "physics_RT.h"

#include <string>

namespace {

constexpr const char *kPhysicsObjectMt = "BallanceTAS.PhysicsObject";
constexpr const char *kCK3dEntityMt = "BallanceTAS.CK3dEntity";
constexpr const char *kVxMatrixMt = "BallanceTAS.VxMatrix";
constexpr const char *kVxVectorMt = "BallanceTAS.VxVector";

PhysicsObject *CheckPhysicsObject(lua_State *L, int index) {
    return tas::lua::CheckUserdata<PhysicsObject>(L, index, kPhysicsObjectMt);
}

VxVector *CheckVxVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxVector>(L, index, kVxVectorMt);
}

void PushVxVector(lua_State *L, const VxVector &value) {
    lua_getglobal(L, "VxVector");
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    lua_pushnumber(L, value.z);
    lua_call(L, 3, 1);
}

void PushVxMatrix(lua_State *L, const VxMatrix &value) {
    tas::lua::PushOwnedUserdata<VxMatrix>(L, kVxMatrixMt, value);
}

void PushCK3dEntity(lua_State *L, CK3dEntity *entity) {
    if (!entity) {
        lua_pushnil(L);
        return;
    }
    tas::lua::PushBorrowedUserdata<CK3dEntity>(L, kCK3dEntityMt, entity);
}

int Name(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    lua_pushstring(L, object && object->GetName() ? object->GetName() : "");
    return 1;
}

int Entity(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    PushCK3dEntity(L, object ? object->GetEntity() : nullptr);
    return 1;
}

int Wake(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    if (object) {
        object->Wake();
    }
    return 0;
}

int IsStatic(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    lua_pushboolean(L, object && object->IsStatic());
    return 1;
}

int Mass(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    lua_pushnumber(L, object ? object->GetMass() : 0.0f);
    return 1;
}

int InvMass(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    lua_pushnumber(L, object ? object->GetInvMass() : 0.0f);
    return 1;
}

int GetInertia(lua_State *L) {
    VxVector inertia;
    CheckPhysicsObject(L, 1)->GetInertia(inertia);
    PushVxVector(L, inertia);
    return 1;
}

int GetInvInertia(lua_State *L) {
    VxVector inertia;
    CheckPhysicsObject(L, 1)->GetInvInertia(inertia);
    PushVxVector(L, inertia);
    return 1;
}

int GetDamping(lua_State *L) {
    float speed = 0.0f;
    float rotation = 0.0f;
    CheckPhysicsObject(L, 1)->GetDamping(&speed, &rotation);
    lua_pushnumber(L, speed);
    lua_pushnumber(L, rotation);
    return 2;
}

int GetDampingSpeed(lua_State *L) {
    float speed = 0.0f;
    CheckPhysicsObject(L, 1)->GetDamping(&speed, nullptr);
    lua_pushnumber(L, speed);
    return 1;
}

int GetDampingRotation(lua_State *L) {
    float rotation = 0.0f;
    CheckPhysicsObject(L, 1)->GetDamping(nullptr, &rotation);
    lua_pushnumber(L, rotation);
    return 1;
}

int GetPosition(lua_State *L) {
    VxVector position;
    CheckPhysicsObject(L, 1)->GetPosition(&position, nullptr);
    PushVxVector(L, position);
    return 1;
}

int GetAngles(lua_State *L) {
    VxVector angles;
    CheckPhysicsObject(L, 1)->GetPosition(nullptr, &angles);
    PushVxVector(L, angles);
    return 1;
}

int GetPositionMatrix(lua_State *L) {
    VxMatrix matrix;
    CheckPhysicsObject(L, 1)->GetPositionMatrix(matrix);
    PushVxMatrix(L, matrix);
    return 1;
}

int GetVelocity(lua_State *L) {
    VxVector velocity;
    VxVector angularVelocity;
    CheckPhysicsObject(L, 1)->GetVelocity(&velocity, &angularVelocity);
    PushVxVector(L, velocity);
    PushVxVector(L, angularVelocity);
    return 2;
}

int GetLinearVelocity(lua_State *L) {
    VxVector velocity;
    CheckPhysicsObject(L, 1)->GetVelocity(&velocity, nullptr);
    PushVxVector(L, velocity);
    return 1;
}

int GetAngularVelocity(lua_State *L) {
    VxVector angularVelocity;
    CheckPhysicsObject(L, 1)->GetVelocity(nullptr, &angularVelocity);
    PushVxVector(L, angularVelocity);
    return 1;
}

int SetVelocity(lua_State *L) {
    auto *linear = CheckVxVector(L, 2);
    auto *angular = lua_isnoneornil(L, 3) ? nullptr : CheckVxVector(L, 3);
    CheckPhysicsObject(L, 1)->SetVelocity(linear, angular);
    return 0;
}

int SetLinearVelocity(lua_State *L) {
    CheckPhysicsObject(L, 1)->SetVelocity(CheckVxVector(L, 2), nullptr);
    return 0;
}

int SetAngularVelocity(lua_State *L) {
    CheckPhysicsObject(L, 1)->SetVelocity(nullptr, CheckVxVector(L, 2));
    return 0;
}

int FrictionCount(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    lua_pushinteger(L, object ? static_cast<lua_Integer>(object->m_FrictionCount) : 0);
    return 1;
}

int ToString(lua_State *L) {
    auto *object = CheckPhysicsObject(L, 1);
    std::string name = object && object->GetName() ? object->GetName() : "unnamed";
    const std::string text = "PhysicsObject('" + name + "')";
    lua_pushlstring(L, text.data(), text.size());
    return 1;
}

} // namespace

void LuaApi::RegisterPhysicsObject(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<PhysicsObject>(state, kPhysicsObjectMt)
        .ReadonlyProperty("name", Name)
        .ReadonlyProperty("entity", Entity)
        .Method("wake", Wake)
        .ReadonlyProperty("is_static", IsStatic)
        .ReadonlyProperty("mass", Mass)
        .ReadonlyProperty("inv_mass", InvMass)
        .Method("get_inertia", GetInertia)
        .Method("get_inv_inertia", GetInvInertia)
        .Method("get_damping", GetDamping)
        .Method("get_damping_speed", GetDampingSpeed)
        .Method("get_damping_rotation", GetDampingRotation)
        .Method("get_position", GetPosition)
        .Method("get_angles", GetAngles)
        .Method("get_position_matrix", GetPositionMatrix)
        .Method("get_velocity", GetVelocity)
        .Method("get_linear_velocity", GetLinearVelocity)
        .Method("get_angular_velocity", GetAngularVelocity)
        .Method("set_velocity", SetVelocity)
        .Method("set_linear_velocity", SetLinearVelocity)
        .Method("set_angular_velocity", SetAngularVelocity)
        .ReadonlyProperty("friction_count", FrictionCount)
        .MetaMethod("__tostring", ToString);
}

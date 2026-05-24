#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <VxMath.h>
#include <cmath>
#include <cstdio>

namespace {

constexpr const char *kVxQuaternionMt = "BallanceTAS.VxQuaternion";
constexpr const char *kVxVectorMt = "BallanceTAS.VxVector";

VxQuaternion *CheckQuat(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxQuaternion>(L, index, kVxQuaternionMt);
}

VxQuaternion *TestQuat(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<VxQuaternion> *>(luaL_testudata(L, index, kVxQuaternionMt));
    return box ? box->ptr : nullptr;
}

VxVector *TestVector(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<VxVector> *>(luaL_testudata(L, index, kVxVectorMt));
    return box ? box->ptr : nullptr;
}

void PushQuat(lua_State *L, const VxQuaternion &value) {
    tas::lua::PushOwnedUserdata<VxQuaternion>(L, kVxQuaternionMt, value);
}

int New(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        PushQuat(L, VxQuaternion());
        return 1;
    }
    if (argc == 2 && TestVector(L, 1)) {
        PushQuat(L, VxQuaternion(*TestVector(L, 1), static_cast<float>(luaL_checknumber(L, 2))));
        return 1;
    }
    if (argc == 4) {
        PushQuat(L, VxQuaternion(static_cast<float>(luaL_checknumber(L, 1)),
                                 static_cast<float>(luaL_checknumber(L, 2)),
                                 static_cast<float>(luaL_checknumber(L, 3)),
                                 static_cast<float>(luaL_checknumber(L, 4))));
        return 1;
    }
    return luaL_error(L, "VxQuaternion(): expected 0 args, VxVector+angle, or 4 numeric args");
}

int Call(lua_State *L) {
    lua_remove(L, 1);
    return New(L);
}

int MagnitudeProperty(lua_State *L) {
    lua_pushnumber(L, Magnitude(*CheckQuat(L, 1)));
    return 1;
}

int ConjugateProperty(lua_State *L) {
    PushQuat(L, Vx3DQuaternionConjugate(*CheckQuat(L, 1)));
    return 1;
}

int EulerAnglesProperty(lua_State *L) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    CheckQuat(L, 1)->ToEulerAngles(&x, &y, &z);
    lua_newtable(L);
    lua_pushnumber(L, x); lua_seti(L, -2, 1);
    lua_pushnumber(L, y); lua_seti(L, -2, 2);
    lua_pushnumber(L, z); lua_seti(L, -2, 3);
    lua_pushnumber(L, x); lua_setfield(L, -2, "x");
    lua_pushnumber(L, y); lua_setfield(L, -2, "y");
    lua_pushnumber(L, z); lua_setfield(L, -2, "z");
    return 1;
}

int ToEulerAngles(lua_State *L) {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    CheckQuat(L, 1)->ToEulerAngles(&x, &y, &z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

int FromRotation(lua_State *L) {
    auto *q = CheckQuat(L, 1);
    auto *axis = TestVector(L, 2);
    if (!axis) {
        return luaL_error(L, "VxQuaternion:from_rotation(axis, angle): expected VxVector axis");
    }
    q->FromRotation(*axis, static_cast<float>(luaL_checknumber(L, 3)));
    return 0;
}

int FromEulerAngles(lua_State *L) {
    CheckQuat(L, 1)->FromEulerAngles(static_cast<float>(luaL_checknumber(L, 2)),
                                     static_cast<float>(luaL_checknumber(L, 3)),
                                     static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

int Normalize(lua_State *L) {
    CheckQuat(L, 1)->Normalize();
    return 0;
}

int Dot(lua_State *L) {
    lua_pushnumber(L, DotProduct(*CheckQuat(L, 1), *CheckQuat(L, 2)));
    return 1;
}

int Slerp(lua_State *L) {
    PushQuat(L, Slerp(static_cast<float>(luaL_checknumber(L, 3)), *CheckQuat(L, 1), *CheckQuat(L, 2)));
    return 1;
}

int LnMethod(lua_State *L) {
    PushQuat(L, Ln(*CheckQuat(L, 1)));
    return 1;
}

int ExpMethod(lua_State *L) {
    PushQuat(L, Exp(*CheckQuat(L, 1)));
    return 1;
}

int LnDifMethod(lua_State *L) {
    PushQuat(L, LnDif(*CheckQuat(L, 1), *CheckQuat(L, 2)));
    return 1;
}

int NumericIndex(lua_State *L) {
    auto *q = CheckQuat(L, 1);
    const int index = static_cast<int>(luaL_checkinteger(L, 2));
    if (index < 0 || index > 3) {
        return luaL_error(L, "VxQuaternion index out of range [0-3]");
    }
    lua_pushnumber(L, (*q)[index]);
    return 1;
}

int NumericNewIndex(lua_State *L) {
    auto *q = CheckQuat(L, 1);
    const int index = static_cast<int>(luaL_checkinteger(L, 2));
    if (index < 0 || index > 3) {
        return luaL_error(L, "VxQuaternion index out of range [0-3]");
    }
    (*q)[index] = static_cast<float>(luaL_checknumber(L, 3));
    return 0;
}

int Add(lua_State *L) { PushQuat(L, *CheckQuat(L, 1) + *CheckQuat(L, 2)); return 1; }
int Sub(lua_State *L) { PushQuat(L, *CheckQuat(L, 1) - *CheckQuat(L, 2)); return 1; }

int Mul(lua_State *L) {
    auto *a = TestQuat(L, 1);
    auto *b = TestQuat(L, 2);
    if (a && b) {
        PushQuat(L, *a * *b);
        return 1;
    }
    if (a && lua_isnumber(L, 2)) {
        PushQuat(L, *a * static_cast<float>(lua_tonumber(L, 2)));
        return 1;
    }
    if (lua_isnumber(L, 1) && b) {
        PushQuat(L, static_cast<float>(lua_tonumber(L, 1)) * *b);
        return 1;
    }
    return luaL_error(L, "VxQuaternion multiplication expects quaternion/quaternion or quaternion/number");
}

int Div(lua_State *L) { PushQuat(L, *CheckQuat(L, 1) / *CheckQuat(L, 2)); return 1; }
int Unm(lua_State *L) { PushQuat(L, -*CheckQuat(L, 1)); return 1; }

int Eq(lua_State *L) {
    auto *a = TestQuat(L, 1);
    auto *b = TestQuat(L, 2);
    lua_pushboolean(L, a && b && *a == *b);
    return 1;
}

int ToString(lua_State *L) {
    auto *q = CheckQuat(L, 1);
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "VxQuaternion(%g, %g, %g, %g)", q->x, q->y, q->z, q->w);
    lua_pushstring(L, buffer);
    return 1;
}

void SetFunction(lua_State *L, const char *name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
}

void RegisterClassTable(lua_State *L) {
    lua_newtable(L);
    SetFunction(L, "new", New);
    lua_newtable(L);
    SetFunction(L, "__call", Call);
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "VxQuaternion");
    lua_getglobal(L, "tas");
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, "VxQuaternion");
    }
    lua_pop(L, 2);
}

} // namespace

void LuaApi::RegisterVxQuaternion(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<VxQuaternion>(state, kVxQuaternionMt)
        .Property<&VxQuaternion::x>("x")
        .Property<&VxQuaternion::y>("y")
        .Property<&VxQuaternion::z>("z")
        .Property<&VxQuaternion::w>("w")
        .ReadonlyProperty("magnitude", MagnitudeProperty)
        .ReadonlyProperty("conjugate", ConjugateProperty)
        .ReadonlyProperty("euler_angles", EulerAnglesProperty)
        .Method("from_rotation", FromRotation)
        .Method("from_euler_angles", FromEulerAngles)
        .Method("to_euler_angles", ToEulerAngles)
        .Method("normalize", Normalize)
        .Method("dot", Dot)
        .Method("slerp", Slerp)
        .Method("ln", LnMethod)
        .Method("exp", ExpMethod)
        .Method("ln_dif", LnDifMethod)
        .NumericIndex(NumericIndex, NumericNewIndex)
        .MetaMethod("__add", Add)
        .MetaMethod("__sub", Sub)
        .MetaMethod("__mul", Mul)
        .MetaMethod("__div", Div)
        .MetaMethod("__unm", Unm)
        .MetaMethod("__eq", Eq)
        .MetaMethod("__tostring", ToString);

    RegisterClassTable(state);
}

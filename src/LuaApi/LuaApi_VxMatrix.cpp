#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <VxMath.h>
#include <cstdio>

namespace {

constexpr const char *kVxMatrixMt = "BallanceTAS.VxMatrix";
constexpr const char *kVxVectorMt = "BallanceTAS.VxVector";
constexpr const char *kVxVector4Mt = "BallanceTAS.VxVector4";

VxMatrix *CheckVxMatrix(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxMatrix>(L, index, kVxMatrixMt);
}

VxVector *CheckVxVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxVector>(L, index, kVxVectorMt);
}

VxVector4 *CheckVxVector4(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxVector4>(L, index, kVxVector4Mt);
}

void PushVxMatrix(lua_State *L, const VxMatrix &matrix) {
    tas::lua::PushOwnedUserdata<VxMatrix>(L, kVxMatrixMt, matrix);
}

void PushVxVector(lua_State *L, const VxVector &value) {
    lua_getglobal(L, "VxVector");
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    lua_pushnumber(L, value.z);
    lua_call(L, 3, 1);
}

void PushVxVector4(lua_State *L, const VxVector4 &value) {
    lua_getglobal(L, "VxVector4");
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    lua_pushnumber(L, value.z);
    lua_pushnumber(L, value.w);
    lua_call(L, 4, 1);
}

int New(lua_State *L) {
    auto matrix = VxMatrix::Identity();
    PushVxMatrix(L, matrix);
    return 1;
}

int Call(lua_State *L) {
    lua_remove(L, 1);
    return New(L);
}

int Determinant(lua_State *L) {
    lua_pushnumber(L, Vx3DMatrixDeterminant(*CheckVxMatrix(L, 1)));
    return 1;
}

int Inverse(lua_State *L) {
    VxMatrix result;
    Vx3DInverseMatrix(result, *CheckVxMatrix(L, 1));
    PushVxMatrix(L, result);
    return 1;
}

int Transpose(lua_State *L) {
    VxMatrix result;
    Vx3DTransposeMatrix(result, *CheckVxMatrix(L, 1));
    PushVxMatrix(L, result);
    return 1;
}

int Clear(lua_State *L) {
    CheckVxMatrix(L, 1)->Clear();
    return 0;
}

int SetIdentity(lua_State *L) {
    CheckVxMatrix(L, 1)->SetIdentity();
    return 0;
}

int Orthographic(lua_State *L) {
    CheckVxMatrix(L, 1)->Orthographic(static_cast<float>(luaL_checknumber(L, 2)),
                                      static_cast<float>(luaL_checknumber(L, 3)),
                                      static_cast<float>(luaL_checknumber(L, 4)),
                                      static_cast<float>(luaL_checknumber(L, 5)));
    return 0;
}

int Perspective(lua_State *L) {
    CheckVxMatrix(L, 1)->Perspective(static_cast<float>(luaL_checknumber(L, 2)),
                                     static_cast<float>(luaL_checknumber(L, 3)),
                                     static_cast<float>(luaL_checknumber(L, 4)),
                                     static_cast<float>(luaL_checknumber(L, 5)));
    return 0;
}

int OrthographicRect(lua_State *L) {
    CheckVxMatrix(L, 1)->OrthographicRect(static_cast<float>(luaL_checknumber(L, 2)),
                                          static_cast<float>(luaL_checknumber(L, 3)),
                                          static_cast<float>(luaL_checknumber(L, 4)),
                                          static_cast<float>(luaL_checknumber(L, 5)),
                                          static_cast<float>(luaL_checknumber(L, 6)),
                                          static_cast<float>(luaL_checknumber(L, 7)));
    return 0;
}

int PerspectiveRect(lua_State *L) {
    CheckVxMatrix(L, 1)->PerspectiveRect(static_cast<float>(luaL_checknumber(L, 2)),
                                         static_cast<float>(luaL_checknumber(L, 3)),
                                         static_cast<float>(luaL_checknumber(L, 4)),
                                         static_cast<float>(luaL_checknumber(L, 5)),
                                         static_cast<float>(luaL_checknumber(L, 6)),
                                         static_cast<float>(luaL_checknumber(L, 7)));
    return 0;
}

int Multiply(lua_State *L) {
    VxMatrix result;
    Vx3DMultiplyMatrix(result, *CheckVxMatrix(L, 1), *CheckVxMatrix(L, 2));
    PushVxMatrix(L, result);
    return 1;
}

int MultiplyVector(lua_State *L) {
    VxVector result;
    Vx3DMultiplyMatrixVector(&result, *CheckVxMatrix(L, 1), CheckVxVector(L, 2));
    PushVxVector(L, result);
    return 1;
}

int MultiplyVector4(lua_State *L) {
    VxVector4 result;
    Vx3DMultiplyMatrixVector4(&result, *CheckVxMatrix(L, 1), CheckVxVector4(L, 2));
    PushVxVector4(L, result);
    return 1;
}

int RotateVector(lua_State *L) {
    VxVector result;
    Vx3DRotateVector(&result, *CheckVxMatrix(L, 1), CheckVxVector(L, 2));
    PushVxVector(L, result);
    return 1;
}

int ToEulerAngles(lua_State *L) {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    Vx3DMatrixToEulerAngles(*CheckVxMatrix(L, 1), &x, &y, &z);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushnumber(L, z);
    return 3;
}

int Interpolate(lua_State *L) {
    VxMatrix result;
    Vx3DInterpolateMatrix(static_cast<float>(luaL_checknumber(L, 3)), result, *CheckVxMatrix(L, 1), *CheckVxMatrix(L, 2));
    PushVxMatrix(L, result);
    return 1;
}

int InterpolateNoScale(lua_State *L) {
    VxMatrix result;
    Vx3DInterpolateMatrixNoScale(static_cast<float>(luaL_checknumber(L, 3)), result, *CheckVxMatrix(L, 1), *CheckVxMatrix(L, 2));
    PushVxMatrix(L, result);
    return 1;
}

int Decompose(lua_State *L) {
    VxQuaternion quat;
    VxVector pos;
    VxVector scale;
    Vx3DDecomposeMatrix(*CheckVxMatrix(L, 1), quat, pos, scale);

    lua_getglobal(L, "VxQuaternion");
    lua_pushnumber(L, quat.x);
    lua_pushnumber(L, quat.y);
    lua_pushnumber(L, quat.z);
    lua_pushnumber(L, quat.w);
    lua_call(L, 4, 1);
    PushVxVector(L, pos);
    PushVxVector(L, scale);
    return 3;
}

int Identity(lua_State *L) {
    PushVxMatrix(L, VxMatrix::Identity());
    return 1;
}

int FromRotation(lua_State *L) {
    VxMatrix result;
    Vx3DMatrixFromRotation(result, *CheckVxVector(L, 1), static_cast<float>(luaL_checknumber(L, 2)));
    PushVxMatrix(L, result);
    return 1;
}

int FromRotationAndOrigin(lua_State *L) {
    VxMatrix result;
    Vx3DMatrixFromRotationAndOrigin(result, *CheckVxVector(L, 1), *CheckVxVector(L, 2), static_cast<float>(luaL_checknumber(L, 3)));
    PushVxMatrix(L, result);
    return 1;
}

int FromEulerAngles(lua_State *L) {
    VxMatrix result;
    Vx3DMatrixFromEulerAngles(result,
                              static_cast<float>(luaL_checknumber(L, 1)),
                              static_cast<float>(luaL_checknumber(L, 2)),
                              static_cast<float>(luaL_checknumber(L, 3)));
    PushVxMatrix(L, result);
    return 1;
}

int Mul(lua_State *L) {
    return Multiply(L);
}

int Eq(lua_State *L) {
    auto *a = static_cast<tas::lua::UserdataBox<VxMatrix> *>(luaL_testudata(L, 1, kVxMatrixMt));
    auto *b = static_cast<tas::lua::UserdataBox<VxMatrix> *>(luaL_testudata(L, 2, kVxMatrixMt));
    lua_pushboolean(L, a && b && a->ptr && b->ptr && *a->ptr == *b->ptr);
    return 1;
}

int Index(lua_State *L) {
    auto *matrix = CheckVxMatrix(L, 1);
    const int row = static_cast<int>(luaL_checkinteger(L, 2));
    if (row < 1 || row > 4) {
        return luaL_error(L, "VxMatrix row index out of range");
    }
    lua_newtable(L);
    for (int column = 0; column < 4; ++column) {
        lua_pushnumber(L, (*matrix)[row - 1][column]);
        lua_rawseti(L, -2, column + 1);
    }
    return 1;
}

int ToString(lua_State *L) {
    auto *m = CheckVxMatrix(L, 1);
    char buffer[384];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "VxMatrix(%g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g, %g)",
                  (*m)[0][0], (*m)[0][1], (*m)[0][2], (*m)[0][3],
                  (*m)[1][0], (*m)[1][1], (*m)[1][2], (*m)[1][3],
                  (*m)[2][0], (*m)[2][1], (*m)[2][2], (*m)[2][3],
                  (*m)[3][0], (*m)[3][1], (*m)[3][2], (*m)[3][3]);
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
    SetFunction(L, "identity", Identity);
    SetFunction(L, "from_rotation", FromRotation);
    SetFunction(L, "from_rotation_and_origin", FromRotationAndOrigin);
    SetFunction(L, "from_euler_angles", FromEulerAngles);
    lua_newtable(L);
    lua_pushcfunction(L, Call);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);
    lua_setglobal(L, "VxMatrix");
}

} // namespace

void LuaApi::RegisterVxMatrix(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<VxMatrix>(state, kVxMatrixMt)
        .ReadonlyProperty("determinant", Determinant)
        .ReadonlyProperty("inverse", Inverse)
        .ReadonlyProperty("transpose", Transpose)
        .Method("clear", Clear)
        .Method("set_identity", SetIdentity)
        .Method("orthographic", Orthographic)
        .Method("perspective", Perspective)
        .Method("orthographic_rect", OrthographicRect)
        .Method("perspective_rect", PerspectiveRect)
        .Method("multiply", Multiply)
        .Method("multiply_vector", MultiplyVector)
        .Method("multiply_vector4", MultiplyVector4)
        .Method("rotate_vector", RotateVector)
        .Method("to_euler_angles", ToEulerAngles)
        .Method("interpolate", Interpolate)
        .Method("interpolate_no_scale", InterpolateNoScale)
        .Method("decompose", Decompose)
        .NumericIndex(Index)
        .MetaMethod("__mul", Mul)
        .MetaMethod("__eq", Eq)
        .MetaMethod("__tostring", ToString);

    RegisterClassTable(state);
}

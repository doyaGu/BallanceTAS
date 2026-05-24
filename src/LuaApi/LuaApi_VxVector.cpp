#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <VxMath.h>
#include <cmath>
#include <cstdio>

namespace {

constexpr const char *kVxVectorMt = "BallanceTAS.VxVector";
constexpr const char *kVxVector4Mt = "BallanceTAS.VxVector4";
constexpr const char *kVx2DVectorMt = "BallanceTAS.Vx2DVector";
constexpr const char *kVxBboxMt = "BallanceTAS.VxBbox";
constexpr const char *kVxCompressedVectorMt = "BallanceTAS.VxCompressedVector";
constexpr const char *kVxCompressedVectorOldMt = "BallanceTAS.VxCompressedVectorOld";

template <typename T>
T *TestUserdata(lua_State *L, int index, const char *metatableName) {
    auto *box = static_cast<tas::lua::UserdataBox<T> *>(luaL_testudata(L, index, metatableName));
    return box ? box->ptr : nullptr;
}

VxVector *CheckVxVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxVector>(L, index, kVxVectorMt);
}

Vx2DVector *CheckVx2DVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<Vx2DVector>(L, index, kVx2DVectorMt);
}

VxVector4 *CheckVxVector4(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxVector4>(L, index, kVxVector4Mt);
}

VxBbox *CheckVxBbox(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxBbox>(L, index, kVxBboxMt);
}

VxCompressedVector *CheckVxCompressedVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxCompressedVector>(L, index, kVxCompressedVectorMt);
}

VxCompressedVectorOld *CheckVxCompressedVectorOld(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxCompressedVectorOld>(L, index, kVxCompressedVectorOldMt);
}

VxVector *TestVxVector(lua_State *L, int index) {
    return TestUserdata<VxVector>(L, index, kVxVectorMt);
}

VxVector4 *TestVxVector4(lua_State *L, int index) {
    return TestUserdata<VxVector4>(L, index, kVxVector4Mt);
}

Vx2DVector *TestVx2DVector(lua_State *L, int index) {
    return TestUserdata<Vx2DVector>(L, index, kVx2DVectorMt);
}

VxBbox *TestVxBbox(lua_State *L, int index) {
    return TestUserdata<VxBbox>(L, index, kVxBboxMt);
}

void PushVxVector(lua_State *L, float x, float y, float z) {
    VxVector value;
    value.x = x;
    value.y = y;
    value.z = z;
    tas::lua::PushOwnedUserdata<VxVector>(L, kVxVectorMt, value);
}

void PushVx2DVector(lua_State *L, float x, float y) {
    Vx2DVector value;
    value.x = x;
    value.y = y;
    tas::lua::PushOwnedUserdata<Vx2DVector>(L, kVx2DVectorMt, value);
}

void PushVxVector4(lua_State *L, float x, float y, float z, float w) {
    VxVector4 value;
    value.x = x;
    value.y = y;
    value.z = z;
    value.w = w;
    tas::lua::PushOwnedUserdata<VxVector4>(L, kVxVector4Mt, value);
}

void PushVxBbox(lua_State *L, const VxVector &min, const VxVector &max) {
    VxBbox value;
    value.Min = min;
    value.Max = max;
    tas::lua::PushOwnedUserdata<VxBbox>(L, kVxBboxMt, value);
}

void PushVxCompressedVector(lua_State *L, float x, float y, float z) {
    VxCompressedVector value;
    value.Set(x, y, z);
    tas::lua::PushOwnedUserdata<VxCompressedVector>(L, kVxCompressedVectorMt, value);
}

void PushVxCompressedVectorOld(lua_State *L, float x, float y, float z) {
    VxCompressedVectorOld value;
    value.Set(x, y, z);
    tas::lua::PushOwnedUserdata<VxCompressedVectorOld>(L, kVxCompressedVectorOldMt, value);
}

bool ReadVectorOperand(lua_State *L, int index, float &x, float &y, float &z) {
    if (auto *value = TestVxVector(L, index)) {
        x = value->x;
        y = value->y;
        z = value->z;
        return true;
    }
    if (lua_isnumber(L, index)) {
        x = y = z = static_cast<float>(lua_tonumber(L, index));
        return true;
    }
    return false;
}

bool ReadVector4Operand(lua_State *L, int index, float &x, float &y, float &z, float &w) {
    if (auto *value = TestVxVector4(L, index)) {
        x = value->x;
        y = value->y;
        z = value->z;
        w = value->w;
        return true;
    }
    if (lua_isnumber(L, index)) {
        x = y = z = w = static_cast<float>(lua_tonumber(L, index));
        return true;
    }
    return false;
}

bool ReadVector2Operand(lua_State *L, int index, float &x, float &y) {
    if (auto *value = TestVx2DVector(L, index)) {
        x = value->x;
        y = value->y;
        return true;
    }
    if (lua_isnumber(L, index)) {
        x = y = static_cast<float>(lua_tonumber(L, index));
        return true;
    }
    return false;
}

float Magnitude(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

float Magnitude(float x, float y, float z, float w) {
    return std::sqrt(x * x + y * y + z * z + w * w);
}

float Magnitude(float x, float y) {
    return std::sqrt(x * x + y * y);
}

int VxVectorNew(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        PushVxVector(L, 0.0f, 0.0f, 0.0f);
        return 1;
    }
    if (argc == 1) {
        const float scalar = static_cast<float>(luaL_checknumber(L, 1));
        PushVxVector(L, scalar, scalar, scalar);
        return 1;
    }
    if (argc == 3) {
        PushVxVector(L,
                     static_cast<float>(luaL_checknumber(L, 1)),
                     static_cast<float>(luaL_checknumber(L, 2)),
                     static_cast<float>(luaL_checknumber(L, 3)));
        return 1;
    }
    return luaL_error(L, "VxVector expects 0, 1, or 3 arguments");
}

int VxVectorCall(lua_State *L) {
    lua_remove(L, 1);
    return VxVectorNew(L);
}

int Vx2DVectorNew(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        PushVx2DVector(L, 0.0f, 0.0f);
        return 1;
    }
    if (argc == 1) {
        const float scalar = static_cast<float>(luaL_checknumber(L, 1));
        PushVx2DVector(L, scalar, scalar);
        return 1;
    }
    if (argc == 2) {
        PushVx2DVector(L,
                       static_cast<float>(luaL_checknumber(L, 1)),
                       static_cast<float>(luaL_checknumber(L, 2)));
        return 1;
    }
    return luaL_error(L, "Vx2DVector expects 0, 1, or 2 arguments");
}

int VxVector4New(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        PushVxVector4(L, 0.0f, 0.0f, 0.0f, 0.0f);
        return 1;
    }
    if (argc == 1) {
        const float scalar = static_cast<float>(luaL_checknumber(L, 1));
        PushVxVector4(L, scalar, scalar, scalar, scalar);
        return 1;
    }
    if (argc == 4) {
        PushVxVector4(L,
                      static_cast<float>(luaL_checknumber(L, 1)),
                      static_cast<float>(luaL_checknumber(L, 2)),
                      static_cast<float>(luaL_checknumber(L, 3)),
                      static_cast<float>(luaL_checknumber(L, 4)));
        return 1;
    }
    return luaL_error(L, "VxVector4 expects 0, 1, or 4 arguments");
}

int VxVector4Call(lua_State *L) {
    lua_remove(L, 1);
    return VxVector4New(L);
}

int VxBboxNew(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        VxVector zero;
        zero.x = zero.y = zero.z = 0.0f;
        PushVxBbox(L, zero, zero);
        return 1;
    }
    if (argc == 1) {
        const float extent = static_cast<float>(luaL_checknumber(L, 1));
        VxVector min;
        min.x = min.y = min.z = -extent;
        VxVector max;
        max.x = max.y = max.z = extent;
        PushVxBbox(L, min, max);
        return 1;
    }
    if (argc == 2) {
        auto *min = CheckVxVector(L, 1);
        auto *max = CheckVxVector(L, 2);
        PushVxBbox(L, *min, *max);
        return 1;
    }
    return luaL_error(L, "VxBbox expects 0, 1, or 2 arguments");
}

int VxBboxCall(lua_State *L) {
    lua_remove(L, 1);
    return VxBboxNew(L);
}

int VxCompressedVectorNew(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        VxCompressedVector value;
        value.xa = 0;
        value.ya = 0;
        tas::lua::PushOwnedUserdata<VxCompressedVector>(L, kVxCompressedVectorMt, value);
        return 1;
    }
    if (argc == 3) {
        PushVxCompressedVector(L,
                               static_cast<float>(luaL_checknumber(L, 1)),
                               static_cast<float>(luaL_checknumber(L, 2)),
                               static_cast<float>(luaL_checknumber(L, 3)));
        return 1;
    }
    return luaL_error(L, "VxCompressedVector expects 0 or 3 arguments");
}

int VxCompressedVectorCall(lua_State *L) {
    lua_remove(L, 1);
    return VxCompressedVectorNew(L);
}

int VxCompressedVectorOldNew(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        VxCompressedVectorOld value;
        value.xa = 0;
        value.ya = 0;
        tas::lua::PushOwnedUserdata<VxCompressedVectorOld>(L, kVxCompressedVectorOldMt, value);
        return 1;
    }
    if (argc == 3) {
        PushVxCompressedVectorOld(L,
                                  static_cast<float>(luaL_checknumber(L, 1)),
                                  static_cast<float>(luaL_checknumber(L, 2)),
                                  static_cast<float>(luaL_checknumber(L, 3)));
        return 1;
    }
    return luaL_error(L, "VxCompressedVectorOld expects 0 or 3 arguments");
}

int VxCompressedVectorOldCall(lua_State *L) {
    lua_remove(L, 1);
    return VxCompressedVectorOldNew(L);
}

int Vx2DVectorCall(lua_State *L) {
    lua_remove(L, 1);
    return Vx2DVectorNew(L);
}

int VxCompressedVectorSet(lua_State *L) {
    auto *value = CheckVxCompressedVector(L, 1);
    value->Set(static_cast<float>(luaL_checknumber(L, 2)),
               static_cast<float>(luaL_checknumber(L, 3)),
               static_cast<float>(luaL_checknumber(L, 4)));
    lua_pushvalue(L, 1);
    return 1;
}

int VxCompressedVectorToString(lua_State *L) {
    auto *value = CheckVxCompressedVector(L, 1);
    char buffer[80];
    std::snprintf(buffer, sizeof(buffer), "VxCompressedVector(%d, %d)", static_cast<int>(value->xa), static_cast<int>(value->ya));
    lua_pushstring(L, buffer);
    return 1;
}

int VxCompressedVectorOldSet(lua_State *L) {
    auto *value = CheckVxCompressedVectorOld(L, 1);
    value->Set(static_cast<float>(luaL_checknumber(L, 2)),
               static_cast<float>(luaL_checknumber(L, 3)),
               static_cast<float>(luaL_checknumber(L, 4)));
    lua_pushvalue(L, 1);
    return 1;
}

int VxCompressedVectorOldToString(lua_State *L) {
    auto *value = CheckVxCompressedVectorOld(L, 1);
    char buffer[88];
    std::snprintf(buffer, sizeof(buffer), "VxCompressedVectorOld(%d, %d)", value->xa, value->ya);
    lua_pushstring(L, buffer);
    return 1;
}

int VxBboxMin(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    PushVxVector(L, box->Min.x, box->Min.y, box->Min.z);
    return 1;
}

int VxBboxSetMin(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    auto *value = CheckVxVector(L, 2);
    box->Min = *value;
    return 0;
}

int VxBboxMax(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    PushVxVector(L, box->Max.x, box->Max.y, box->Max.z);
    return 1;
}

int VxBboxSetMax(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    auto *value = CheckVxVector(L, 2);
    box->Max = *value;
    return 0;
}

int VxBboxSize(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    const VxVector value = box->GetSize();
    PushVxVector(L, value.x, value.y, value.z);
    return 1;
}

int VxBboxHalfSize(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    const VxVector value = box->GetHalfSize();
    PushVxVector(L, value.x, value.y, value.z);
    return 1;
}

int VxBboxCenter(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    const VxVector value = box->GetCenter();
    PushVxVector(L, value.x, value.y, value.z);
    return 1;
}

int VxBboxSetCenterProperty(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    auto *center = CheckVxVector(L, 2);
    const VxVector halfSize = box->GetHalfSize();
    box->SetCenter(*center, halfSize);
    return 0;
}

int VxBboxIsValid(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    lua_pushboolean(L, box->IsValid());
    return 1;
}

int VxBboxSetCorners(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    auto *min = CheckVxVector(L, 2);
    auto *max = CheckVxVector(L, 3);
    box->SetCorners(*min, *max);
    lua_pushvalue(L, 1);
    return 1;
}

int VxBboxSetCenter(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    auto *center = CheckVxVector(L, 2);
    auto *halfSize = CheckVxVector(L, 3);
    box->SetCenter(*center, *halfSize);
    lua_pushvalue(L, 1);
    return 1;
}

int VxBboxReset(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    box->Reset();
    lua_pushvalue(L, 1);
    return 1;
}

int VxBboxMerge(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    if (auto *other = TestVxBbox(L, 2)) {
        box->Merge(*other);
    } else {
        auto *point = CheckVxVector(L, 2);
        box->Merge(*point);
    }
    lua_pushvalue(L, 1);
    return 1;
}

int VxBboxEq(lua_State *L) {
    auto *a = TestVxBbox(L, 1);
    auto *b = TestVxBbox(L, 2);
    lua_pushboolean(L, a && b && *a == *b);
    return 1;
}

int VxBboxToString(lua_State *L) {
    auto *box = CheckVxBbox(L, 1);
    char buffer[160];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "VxBbox(min=%g,%g,%g, max=%g,%g,%g)",
                  box->Min.x,
                  box->Min.y,
                  box->Min.z,
                  box->Max.x,
                  box->Max.y,
                  box->Max.z);
    lua_pushstring(L, buffer);
    return 1;
}

int VxVector4Magnitude(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    lua_pushnumber(L, Magnitude(value->x, value->y, value->z, value->w));
    return 1;
}

int VxVector4SquareMagnitude(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    lua_pushnumber(L, value->x * value->x + value->y * value->y + value->z * value->z + value->w * value->w);
    return 1;
}

int VxVector4NumericIndex(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    switch (luaL_checkinteger(L, 2)) {
    case 0:
        lua_pushnumber(L, value->x);
        return 1;
    case 1:
        lua_pushnumber(L, value->y);
        return 1;
    case 2:
        lua_pushnumber(L, value->z);
        return 1;
    case 3:
        lua_pushnumber(L, value->w);
        return 1;
    default:
        lua_pushnil(L);
        return 1;
    }
}

int VxVector4NumericNewIndex(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    const float number = static_cast<float>(luaL_checknumber(L, 3));
    switch (luaL_checkinteger(L, 2)) {
    case 0:
        value->x = number;
        return 0;
    case 1:
        value->y = number;
        return 0;
    case 2:
        value->z = number;
        return 0;
    case 3:
        value->w = number;
        return 0;
    default:
        return luaL_error(L, "VxVector4 index out of range");
    }
}

int VxVector4Set(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    value->x = static_cast<float>(luaL_checknumber(L, 2));
    value->y = static_cast<float>(luaL_checknumber(L, 3));
    value->z = static_cast<float>(luaL_checknumber(L, 4));
    value->w = static_cast<float>(luaL_checknumber(L, 5));
    lua_pushvalue(L, 1);
    return 1;
}

int VxVector4Normalize(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    const float magnitude = Magnitude(value->x, value->y, value->z, value->w);
    if (magnitude > 0.0f) {
        value->x /= magnitude;
        value->y /= magnitude;
        value->z /= magnitude;
        value->w /= magnitude;
    }
    lua_pushvalue(L, 1);
    return 1;
}

int VxVector4Dot(lua_State *L) {
    auto *a = CheckVxVector4(L, 1);
    auto *b = CheckVxVector4(L, 2);
    lua_pushnumber(L, a->x * b->x + a->y * b->y + a->z * b->z + a->w * b->w);
    return 1;
}

template <typename BinaryOp>
int VxVector4Binary(lua_State *L, const char *operationName, BinaryOp op) {
    float ax, ay, az, aw;
    float bx, by, bz, bw;
    if (!ReadVector4Operand(L, 1, ax, ay, az, aw) || !ReadVector4Operand(L, 2, bx, by, bz, bw)) {
        return luaL_error(L, "%s expects VxVector4 or number operands", operationName);
    }
    PushVxVector4(L, op(ax, bx), op(ay, by), op(az, bz), op(aw, bw));
    return 1;
}

int VxVector4Add(lua_State *L) {
    return VxVector4Binary(L, "VxVector4 addition", [](float a, float b) { return a + b; });
}

int VxVector4Sub(lua_State *L) {
    return VxVector4Binary(L, "VxVector4 subtraction", [](float a, float b) { return a - b; });
}

int VxVector4Mul(lua_State *L) {
    return VxVector4Binary(L, "VxVector4 multiplication", [](float a, float b) { return a * b; });
}

int VxVector4Div(lua_State *L) {
    return VxVector4Binary(L, "VxVector4 division", [](float a, float b) { return a / b; });
}

int VxVector4Unm(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    PushVxVector4(L, -value->x, -value->y, -value->z, -value->w);
    return 1;
}

int VxVector4Eq(lua_State *L) {
    auto *a = TestVxVector4(L, 1);
    auto *b = TestVxVector4(L, 2);
    lua_pushboolean(L, a && b && a->x == b->x && a->y == b->y && a->z == b->z && a->w == b->w);
    return 1;
}

int VxVector4ToString(lua_State *L) {
    auto *value = CheckVxVector4(L, 1);
    char buffer[112];
    std::snprintf(buffer, sizeof(buffer), "VxVector4(%g, %g, %g, %g)", value->x, value->y, value->z, value->w);
    lua_pushstring(L, buffer);
    return 1;
}

int VxVectorMagnitude(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    lua_pushnumber(L, Magnitude(value->x, value->y, value->z));
    return 1;
}

int VxVectorSquareMagnitude(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    lua_pushnumber(L, value->x * value->x + value->y * value->y + value->z * value->z);
    return 1;
}

int VxVectorNumericIndex(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    switch (luaL_checkinteger(L, 2)) {
    case 0:
        lua_pushnumber(L, value->x);
        return 1;
    case 1:
        lua_pushnumber(L, value->y);
        return 1;
    case 2:
        lua_pushnumber(L, value->z);
        return 1;
    default:
        lua_pushnil(L);
        return 1;
    }
}

int VxVectorNumericNewIndex(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    const float number = static_cast<float>(luaL_checknumber(L, 3));
    switch (luaL_checkinteger(L, 2)) {
    case 0:
        value->x = number;
        return 0;
    case 1:
        value->y = number;
        return 0;
    case 2:
        value->z = number;
        return 0;
    default:
        return luaL_error(L, "VxVector index out of range");
    }
}

int VxVectorSet(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    value->x = static_cast<float>(luaL_checknumber(L, 2));
    value->y = static_cast<float>(luaL_checknumber(L, 3));
    value->z = static_cast<float>(luaL_checknumber(L, 4));
    lua_pushvalue(L, 1);
    return 1;
}

int VxVectorNormalize(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    const float magnitude = Magnitude(value->x, value->y, value->z);
    if (magnitude > 0.0f) {
        value->x /= magnitude;
        value->y /= magnitude;
        value->z /= magnitude;
    }
    lua_pushvalue(L, 1);
    return 1;
}

int VxVectorDot(lua_State *L) {
    auto *a = CheckVxVector(L, 1);
    auto *b = CheckVxVector(L, 2);
    lua_pushnumber(L, a->x * b->x + a->y * b->y + a->z * b->z);
    return 1;
}

int VxVectorCross(lua_State *L) {
    auto *a = CheckVxVector(L, 1);
    auto *b = CheckVxVector(L, 2);
    PushVxVector(L,
                 a->y * b->z - a->z * b->y,
                 a->z * b->x - a->x * b->z,
                 a->x * b->y - a->y * b->x);
    return 1;
}

template <typename BinaryOp>
int VxVectorBinary(lua_State *L, const char *operationName, BinaryOp op) {
    float ax, ay, az;
    float bx, by, bz;
    if (!ReadVectorOperand(L, 1, ax, ay, az) || !ReadVectorOperand(L, 2, bx, by, bz)) {
        return luaL_error(L, "%s expects VxVector or number operands", operationName);
    }
    PushVxVector(L, op(ax, bx), op(ay, by), op(az, bz));
    return 1;
}

int VxVectorAdd(lua_State *L) {
    return VxVectorBinary(L, "VxVector addition", [](float a, float b) { return a + b; });
}

int VxVectorSub(lua_State *L) {
    return VxVectorBinary(L, "VxVector subtraction", [](float a, float b) { return a - b; });
}

int VxVectorMul(lua_State *L) {
    return VxVectorBinary(L, "VxVector multiplication", [](float a, float b) { return a * b; });
}

int VxVectorDiv(lua_State *L) {
    return VxVectorBinary(L, "VxVector division", [](float a, float b) { return a / b; });
}

int VxVectorUnm(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    PushVxVector(L, -value->x, -value->y, -value->z);
    return 1;
}

int VxVectorEq(lua_State *L) {
    auto *a = TestVxVector(L, 1);
    auto *b = TestVxVector(L, 2);
    lua_pushboolean(L, a && b && a->x == b->x && a->y == b->y && a->z == b->z);
    return 1;
}

int VxVectorToString(lua_State *L) {
    auto *value = CheckVxVector(L, 1);
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "VxVector(%g, %g, %g)", value->x, value->y, value->z);
    lua_pushstring(L, buffer);
    return 1;
}

int VxVectorAxisX(lua_State *L) {
    PushVxVector(L, 1.0f, 0.0f, 0.0f);
    return 1;
}

int VxVectorAxisY(lua_State *L) {
    PushVxVector(L, 0.0f, 1.0f, 0.0f);
    return 1;
}

int VxVectorAxisZ(lua_State *L) {
    PushVxVector(L, 0.0f, 0.0f, 1.0f);
    return 1;
}

int Vx2DVectorMagnitude(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    lua_pushnumber(L, Magnitude(value->x, value->y));
    return 1;
}

int Vx2DVectorSquareMagnitude(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    lua_pushnumber(L, value->x * value->x + value->y * value->y);
    return 1;
}

int Vx2DVectorNumericIndex(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    switch (luaL_checkinteger(L, 2)) {
    case 0:
        lua_pushnumber(L, value->x);
        return 1;
    case 1:
        lua_pushnumber(L, value->y);
        return 1;
    default:
        lua_pushnil(L);
        return 1;
    }
}

int Vx2DVectorNumericNewIndex(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    const float number = static_cast<float>(luaL_checknumber(L, 3));
    switch (luaL_checkinteger(L, 2)) {
    case 0:
        value->x = number;
        return 0;
    case 1:
        value->y = number;
        return 0;
    default:
        return luaL_error(L, "Vx2DVector index out of range");
    }
}

int Vx2DVectorSet(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    value->x = static_cast<float>(luaL_checknumber(L, 2));
    value->y = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushvalue(L, 1);
    return 1;
}

int Vx2DVectorNormalize(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    const float magnitude = Magnitude(value->x, value->y);
    if (magnitude > 0.0f) {
        value->x /= magnitude;
        value->y /= magnitude;
    }
    lua_pushvalue(L, 1);
    return 1;
}

int Vx2DVectorDot(lua_State *L) {
    auto *a = CheckVx2DVector(L, 1);
    auto *b = CheckVx2DVector(L, 2);
    lua_pushnumber(L, a->x * b->x + a->y * b->y);
    return 1;
}

template <typename BinaryOp>
int Vx2DVectorBinary(lua_State *L, const char *operationName, BinaryOp op) {
    float ax, ay;
    float bx, by;
    if (!ReadVector2Operand(L, 1, ax, ay) || !ReadVector2Operand(L, 2, bx, by)) {
        return luaL_error(L, "%s expects Vx2DVector or number operands", operationName);
    }
    PushVx2DVector(L, op(ax, bx), op(ay, by));
    return 1;
}

int Vx2DVectorAdd(lua_State *L) {
    return Vx2DVectorBinary(L, "Vx2DVector addition", [](float a, float b) { return a + b; });
}

int Vx2DVectorSub(lua_State *L) {
    return Vx2DVectorBinary(L, "Vx2DVector subtraction", [](float a, float b) { return a - b; });
}

int Vx2DVectorMul(lua_State *L) {
    return Vx2DVectorBinary(L, "Vx2DVector multiplication", [](float a, float b) { return a * b; });
}

int Vx2DVectorDiv(lua_State *L) {
    return Vx2DVectorBinary(L, "Vx2DVector division", [](float a, float b) { return a / b; });
}

int Vx2DVectorUnm(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    PushVx2DVector(L, -value->x, -value->y);
    return 1;
}

int Vx2DVectorEq(lua_State *L) {
    auto *a = TestVx2DVector(L, 1);
    auto *b = TestVx2DVector(L, 2);
    lua_pushboolean(L, a && b && a->x == b->x && a->y == b->y);
    return 1;
}

int Vx2DVectorToString(lua_State *L) {
    auto *value = CheckVx2DVector(L, 1);
    char buffer[80];
    std::snprintf(buffer, sizeof(buffer), "Vx2DVector(%g, %g)", value->x, value->y);
    lua_pushstring(L, buffer);
    return 1;
}

void SetFunction(lua_State *L, const char *name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
}

void RegisterClassTable(lua_State *L, const char *name, lua_CFunction constructor, lua_CFunction callConstructor) {
    lua_newtable(L);
    SetFunction(L, "new", constructor);

    lua_newtable(L);
    SetFunction(L, "__call", callConstructor);
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setglobal(L, name);

    lua_getglobal(L, "tas");
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, name);
    }
    lua_pop(L, 2);
}

void RegisterVxVectorType(lua_State *L) {
    tas::lua::LuaUserdataRegistry<VxVector>(L, kVxVectorMt)
        .Property<&VxVector::x>("x")
        .Property<&VxVector::y>("y")
        .Property<&VxVector::z>("z")
        .ReadonlyProperty("magnitude", VxVectorMagnitude)
        .ReadonlyProperty("square_magnitude", VxVectorSquareMagnitude)
        .Method("set", VxVectorSet)
        .Method("normalize", VxVectorNormalize)
        .Method("dot", VxVectorDot)
        .Method("cross", VxVectorCross)
        .NumericIndex(VxVectorNumericIndex, VxVectorNumericNewIndex)
        .MetaMethod("__add", VxVectorAdd)
        .MetaMethod("__sub", VxVectorSub)
        .MetaMethod("__mul", VxVectorMul)
        .MetaMethod("__div", VxVectorDiv)
        .MetaMethod("__unm", VxVectorUnm)
        .MetaMethod("__eq", VxVectorEq)
        .MetaMethod("__tostring", VxVectorToString);

    RegisterClassTable(L, "VxVector", VxVectorNew, VxVectorCall);
    lua_getglobal(L, "VxVector");
    SetFunction(L, "axis_x", VxVectorAxisX);
    SetFunction(L, "axis_y", VxVectorAxisY);
    SetFunction(L, "axis_z", VxVectorAxisZ);
    lua_pop(L, 1);
}

void RegisterVx2DVectorType(lua_State *L) {
    tas::lua::LuaUserdataRegistry<Vx2DVector>(L, kVx2DVectorMt)
        .Property<&Vx2DVector::x>("x")
        .Property<&Vx2DVector::y>("y")
        .ReadonlyProperty("magnitude", Vx2DVectorMagnitude)
        .ReadonlyProperty("square_magnitude", Vx2DVectorSquareMagnitude)
        .Method("set", Vx2DVectorSet)
        .Method("normalize", Vx2DVectorNormalize)
        .Method("dot", Vx2DVectorDot)
        .NumericIndex(Vx2DVectorNumericIndex, Vx2DVectorNumericNewIndex)
        .MetaMethod("__add", Vx2DVectorAdd)
        .MetaMethod("__sub", Vx2DVectorSub)
        .MetaMethod("__mul", Vx2DVectorMul)
        .MetaMethod("__div", Vx2DVectorDiv)
        .MetaMethod("__unm", Vx2DVectorUnm)
        .MetaMethod("__eq", Vx2DVectorEq)
        .MetaMethod("__tostring", Vx2DVectorToString);

    RegisterClassTable(L, "Vx2DVector", Vx2DVectorNew, Vx2DVectorCall);
}

void RegisterVxVector4Type(lua_State *L) {
    tas::lua::LuaUserdataRegistry<VxVector4>(L, kVxVector4Mt)
        .Property<&VxVector4::x>("x")
        .Property<&VxVector4::y>("y")
        .Property<&VxVector4::z>("z")
        .Property<&VxVector4::w>("w")
        .ReadonlyProperty("magnitude", VxVector4Magnitude)
        .ReadonlyProperty("square_magnitude", VxVector4SquareMagnitude)
        .Method("set", VxVector4Set)
        .Method("normalize", VxVector4Normalize)
        .Method("dot", VxVector4Dot)
        .NumericIndex(VxVector4NumericIndex, VxVector4NumericNewIndex)
        .MetaMethod("__add", VxVector4Add)
        .MetaMethod("__sub", VxVector4Sub)
        .MetaMethod("__mul", VxVector4Mul)
        .MetaMethod("__div", VxVector4Div)
        .MetaMethod("__unm", VxVector4Unm)
        .MetaMethod("__eq", VxVector4Eq)
        .MetaMethod("__tostring", VxVector4ToString);

    RegisterClassTable(L, "VxVector4", VxVector4New, VxVector4Call);
}

void RegisterVxBboxType(lua_State *L) {
    tas::lua::LuaUserdataRegistry<VxBbox>(L, kVxBboxMt)
        .Property("min", VxBboxMin, VxBboxSetMin)
        .Property("max", VxBboxMax, VxBboxSetMax)
        .Property("center", VxBboxCenter, VxBboxSetCenterProperty)
        .ReadonlyProperty("size", VxBboxSize)
        .ReadonlyProperty("half_size", VxBboxHalfSize)
        .Method("is_valid", VxBboxIsValid)
        .Method("set_corners", VxBboxSetCorners)
        .Method("set_center", VxBboxSetCenter)
        .Method("reset", VxBboxReset)
        .Method("merge", VxBboxMerge)
        .MetaMethod("__eq", VxBboxEq)
        .MetaMethod("__tostring", VxBboxToString);

    RegisterClassTable(L, "VxBbox", VxBboxNew, VxBboxCall);
}

void RegisterVxCompressedVectorType(lua_State *L) {
    tas::lua::LuaUserdataRegistry<VxCompressedVector>(L, kVxCompressedVectorMt)
        .Property<&VxCompressedVector::xa>("xa")
        .Property<&VxCompressedVector::ya>("ya")
        .Method("set", VxCompressedVectorSet)
        .MetaMethod("__tostring", VxCompressedVectorToString);

    RegisterClassTable(L, "VxCompressedVector", VxCompressedVectorNew, VxCompressedVectorCall);
}

void RegisterVxCompressedVectorOldType(lua_State *L) {
    tas::lua::LuaUserdataRegistry<VxCompressedVectorOld>(L, kVxCompressedVectorOldMt)
        .Property<&VxCompressedVectorOld::xa>("xa")
        .Property<&VxCompressedVectorOld::ya>("ya")
        .Method("set", VxCompressedVectorOldSet)
        .MetaMethod("__tostring", VxCompressedVectorOldToString);

    RegisterClassTable(L, "VxCompressedVectorOld", VxCompressedVectorOldNew, VxCompressedVectorOldCall);
}

} // namespace

void LuaApi::RegisterVxVector(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);
    RegisterVxVectorType(state);
    RegisterVxVector4Type(state);
    RegisterVx2DVectorType(state);
    RegisterVxBboxType(state);
    RegisterVxCompressedVectorType(state);
    RegisterVxCompressedVectorOldType(state);
}

#include "LuaApi.h"

#include "../LuaRuntime/LuaStackGuard.h"
#include "../LuaRuntime/LuaUserdata.h"

#include <VxMath.h>
#include <cstdio>

constexpr const char *kVxRectMt = "BallanceTAS.VxRect";
constexpr const char *kVx2DVectorMt = "BallanceTAS.Vx2DVector";

static VxRect *CheckVxRect(lua_State *L, int index) {
    return tas::lua::CheckUserdata<VxRect>(L, index, kVxRectMt);
}

static VxRect *TestVxRect(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<VxRect> *>(luaL_testudata(L, index, kVxRectMt));
    return box ? box->ptr : nullptr;
}

static Vx2DVector *CheckVx2DVector(lua_State *L, int index) {
    return tas::lua::CheckUserdata<Vx2DVector>(L, index, kVx2DVectorMt);
}

static Vx2DVector *TestVx2DVector(lua_State *L, int index) {
    auto *box = static_cast<tas::lua::UserdataBox<Vx2DVector> *>(luaL_testudata(L, index, kVx2DVectorMt));
    return box ? box->ptr : nullptr;
}

static void PushVxRect(lua_State *L, const VxRect &value) {
    tas::lua::PushOwnedUserdata<VxRect>(L, kVxRectMt, value);
}

static void PushVx2DVector(lua_State *L, const Vx2DVector &value) {
    tas::lua::PushOwnedUserdata<Vx2DVector>(L, kVx2DVectorMt, value);
}

static int VxRectNew(lua_State *L) {
    const int argc = lua_gettop(L);
    if (argc == 0) {
        PushVxRect(L, VxRect());
        return 1;
    }
    if (argc == 2 && TestVx2DVector(L, 1) && TestVx2DVector(L, 2)) {
        PushVxRect(L, VxRect(*CheckVx2DVector(L, 1), *CheckVx2DVector(L, 2)));
        return 1;
    }
    if (argc == 4) {
        PushVxRect(L, VxRect(static_cast<float>(luaL_checknumber(L, 1)),
                             static_cast<float>(luaL_checknumber(L, 2)),
                             static_cast<float>(luaL_checknumber(L, 3)),
                             static_cast<float>(luaL_checknumber(L, 4))));
        return 1;
    }
    return luaL_error(L, "VxRect(): expected 0 args, 2 Vx2DVector args, or 4 numeric args");
}

static int VxRectCall(lua_State *L) {
    lua_remove(L, 1);
    return VxRectNew(L);
}

static int Width(lua_State *L) { lua_pushnumber(L, CheckVxRect(L, 1)->GetWidth()); return 1; }
static int SetWidth(lua_State *L) { CheckVxRect(L, 1)->SetWidth(static_cast<float>(luaL_checknumber(L, 2))); return 0; }
static int Height(lua_State *L) { lua_pushnumber(L, CheckVxRect(L, 1)->GetHeight()); return 1; }
static int SetHeight(lua_State *L) { CheckVxRect(L, 1)->SetHeight(static_cast<float>(luaL_checknumber(L, 2))); return 0; }
static int HCenter(lua_State *L) { lua_pushnumber(L, CheckVxRect(L, 1)->GetHCenter()); return 1; }
static int VCenter(lua_State *L) { lua_pushnumber(L, CheckVxRect(L, 1)->GetVCenter()); return 1; }

static int Size(lua_State *L) { PushVx2DVector(L, CheckVxRect(L, 1)->GetSize()); return 1; }
static int SetSize(lua_State *L) { CheckVxRect(L, 1)->SetSize(*CheckVx2DVector(L, 2)); return 0; }
static int HalfSize(lua_State *L) { PushVx2DVector(L, CheckVxRect(L, 1)->GetHalfSize()); return 1; }
static int SetHalfSize(lua_State *L) { CheckVxRect(L, 1)->SetHalfSize(*CheckVx2DVector(L, 2)); return 0; }
static int Center(lua_State *L) { PushVx2DVector(L, CheckVxRect(L, 1)->GetCenter()); return 1; }
static int SetCenterProperty(lua_State *L) { CheckVxRect(L, 1)->SetCenter(*CheckVx2DVector(L, 2)); return 0; }
static int TopLeft(lua_State *L) { PushVx2DVector(L, CheckVxRect(L, 1)->GetTopLeft()); return 1; }
static int SetTopLeft(lua_State *L) { CheckVxRect(L, 1)->SetTopLeft(*CheckVx2DVector(L, 2)); return 0; }
static int BottomRight(lua_State *L) { PushVx2DVector(L, CheckVxRect(L, 1)->GetBottomRight()); return 1; }
static int SetBottomRight(lua_State *L) { CheckVxRect(L, 1)->SetBottomRight(*CheckVx2DVector(L, 2)); return 0; }

static int Clear(lua_State *L) { CheckVxRect(L, 1)->Clear(); return 0; }

static int SetCorners(lua_State *L) {
    auto *rect = CheckVxRect(L, 1);
    if (lua_gettop(L) == 3 && TestVx2DVector(L, 2) && TestVx2DVector(L, 3)) {
        rect->SetCorners(*CheckVx2DVector(L, 2), *CheckVx2DVector(L, 3));
        return 0;
    }
    if (lua_gettop(L) == 5) {
        rect->SetCorners(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                         static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)));
        return 0;
    }
    return luaL_error(L, "VxRect:set_corners(): expected 2 Vx2DVector args or 4 numeric args");
}

static int SetDimension(lua_State *L) {
    auto *rect = CheckVxRect(L, 1);
    if (lua_gettop(L) == 3 && TestVx2DVector(L, 2) && TestVx2DVector(L, 3)) {
        rect->SetDimension(*CheckVx2DVector(L, 2), *CheckVx2DVector(L, 3));
        return 0;
    }
    if (lua_gettop(L) == 5) {
        rect->SetDimension(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                           static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)));
        return 0;
    }
    return luaL_error(L, "VxRect:set_dimension(): expected 2 Vx2DVector args or 4 numeric args");
}

static int SetCenter(lua_State *L) {
    auto *rect = CheckVxRect(L, 1);
    if (lua_gettop(L) == 3 && TestVx2DVector(L, 2) && TestVx2DVector(L, 3)) {
        rect->SetCenter(*CheckVx2DVector(L, 2), *CheckVx2DVector(L, 3));
        return 0;
    }
    if (lua_gettop(L) == 5) {
        rect->SetCenter(static_cast<float>(luaL_checknumber(L, 2)), static_cast<float>(luaL_checknumber(L, 3)),
                        static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)));
        return 0;
    }
    return luaL_error(L, "VxRect:set_center(): expected 2 Vx2DVector args or 4 numeric args");
}

static bool ReadVector2(lua_State *L, int index, Vx2DVector &out) {
    if (auto *value = TestVx2DVector(L, index)) {
        out = *value;
        return true;
    }
    if (lua_isnumber(L, index) && lua_isnumber(L, index + 1)) {
        out.x = static_cast<float>(lua_tonumber(L, index));
        out.y = static_cast<float>(lua_tonumber(L, index + 1));
        return true;
    }
    return false;
}

static int Bounding(lua_State *L) {
    Vx2DVector p1;
    Vx2DVector p2;
    if (lua_gettop(L) == 3 && TestVx2DVector(L, 2) && TestVx2DVector(L, 3)) {
        CheckVxRect(L, 1)->Bounding(*CheckVx2DVector(L, 2), *CheckVx2DVector(L, 3));
        return 0;
    }
    if (lua_gettop(L) == 5 && ReadVector2(L, 2, p1) && ReadVector2(L, 4, p2)) {
        CheckVxRect(L, 1)->Bounding(p1, p2);
        return 0;
    }
    return luaL_error(L, "VxRect:bounding(): expected 2 Vx2DVector args or 4 numeric args");
}
static int Normalize(lua_State *L) { CheckVxRect(L, 1)->Normalize(); return 0; }
static int Move(lua_State *L) {
    Vx2DVector value;
    if (!ReadVector2(L, 2, value)) return luaL_error(L, "VxRect:move(): expected Vx2DVector or x, y");
    CheckVxRect(L, 1)->Move(value);
    return 0;
}
static int Translate(lua_State *L) {
    Vx2DVector value;
    if (!ReadVector2(L, 2, value)) return luaL_error(L, "VxRect:translate(): expected Vx2DVector or x, y");
    CheckVxRect(L, 1)->Translate(value);
    return 0;
}
static int HMove(lua_State *L) { CheckVxRect(L, 1)->HMove(static_cast<float>(luaL_checknumber(L, 2))); return 0; }
static int VMove(lua_State *L) { CheckVxRect(L, 1)->VMove(static_cast<float>(luaL_checknumber(L, 2))); return 0; }
static int HTranslate(lua_State *L) { CheckVxRect(L, 1)->HTranslate(static_cast<float>(luaL_checknumber(L, 2))); return 0; }
static int VTranslate(lua_State *L) { CheckVxRect(L, 1)->VTranslate(static_cast<float>(luaL_checknumber(L, 2))); return 0; }
static int Scale(lua_State *L) {
    Vx2DVector value;
    if (!ReadVector2(L, 2, value)) return luaL_error(L, "VxRect:scale(): expected Vx2DVector or x, y");
    CheckVxRect(L, 1)->Scale(value);
    return 0;
}
static int Inflate(lua_State *L) {
    Vx2DVector value;
    if (!ReadVector2(L, 2, value)) return luaL_error(L, "VxRect:inflate(): expected Vx2DVector or x, y");
    CheckVxRect(L, 1)->Inflate(value);
    return 0;
}
static int Merge(lua_State *L) { CheckVxRect(L, 1)->Merge(*CheckVxRect(L, 2)); return 0; }

static int IsInside(lua_State *L) {
    auto *rect = CheckVxRect(L, 1);
    if (auto *other = TestVxRect(L, 2)) {
        lua_pushboolean(L, rect->IsInside(*other));
        return 1;
    }
    if (auto *point = TestVx2DVector(L, 2)) {
        lua_pushboolean(L, rect->IsInside(*point));
        return 1;
    }
    return luaL_error(L, "VxRect:is_inside(): expected VxRect or Vx2DVector");
}

static int IsOutside(lua_State *L) { lua_pushboolean(L, CheckVxRect(L, 1)->IsOutside(*CheckVxRect(L, 2))); return 1; }
static int IsNull(lua_State *L) { lua_pushboolean(L, CheckVxRect(L, 1)->IsNull()); return 1; }
static int IsEmpty(lua_State *L) { lua_pushboolean(L, CheckVxRect(L, 1)->IsEmpty()); return 1; }
static int Clip(lua_State *L) { lua_pushboolean(L, CheckVxRect(L, 1)->Clip(*CheckVxRect(L, 2))); return 1; }

static int Eq(lua_State *L) {
    auto *a = TestVxRect(L, 1);
    auto *b = TestVxRect(L, 2);
    lua_pushboolean(L, a && b && *a == *b);
    return 1;
}

static int ToString(lua_State *L) {
    auto *rect = CheckVxRect(L, 1);
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "VxRect(%g, %g, %g, %g)", rect->left, rect->top, rect->right, rect->bottom);
    lua_pushstring(L, buffer);
    return 1;
}

static void SetFunction(lua_State *L, const char *name, lua_CFunction function) {
    lua_pushcfunction(L, function);
    lua_setfield(L, -2, name);
}

static void RegisterClassTable(lua_State *L) {
    lua_newtable(L);
    SetFunction(L, "new", VxRectNew);
    lua_newtable(L);
    SetFunction(L, "__call", VxRectCall);
    lua_setmetatable(L, -2);
    lua_pushvalue(L, -1);
    lua_setglobal(L, "VxRect");
    lua_getglobal(L, "tas");
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, "VxRect");
    }
    lua_pop(L, 2);
}

void LuaApi::RegisterVxRect(lua_State *state) {
    tas::lua::LuaStackGuard guard(state);

    tas::lua::LuaUserdataRegistry<VxRect>(state, kVxRectMt)
        .Property<&VxRect::left>("left")
        .Property<&VxRect::top>("top")
        .Property<&VxRect::right>("right")
        .Property<&VxRect::bottom>("bottom")
        .Property("width", Width, SetWidth)
        .Property("height", Height, SetHeight)
        .ReadonlyProperty("h_center", HCenter)
        .ReadonlyProperty("v_center", VCenter)
        .Property("size", Size, SetSize)
        .Property("half_size", HalfSize, SetHalfSize)
        .Property("center", Center, SetCenterProperty)
        .Property("top_left", TopLeft, SetTopLeft)
        .Property("bottom_right", BottomRight, SetBottomRight)
        .Method("clear", Clear)
        .Method("set_corners", SetCorners)
        .Method("set_dimension", SetDimension)
        .Method("set_center", SetCenter)
        .Method("bounding", Bounding)
        .Method("normalize", Normalize)
        .Method("move", Move)
        .Method("translate", Translate)
        .Method("h_move", HMove)
        .Method("v_move", VMove)
        .Method("h_translate", HTranslate)
        .Method("v_translate", VTranslate)
        .Method("scale", Scale)
        .Method("inflate", Inflate)
        .Method("merge", Merge)
        .Method("is_inside", IsInside)
        .Method("is_outside", IsOutside)
        .Method("is_null", IsNull)
        .Method("is_empty", IsEmpty)
        .Method("clip", Clip)
        .MetaMethod("__eq", Eq)
        .MetaMethod("__tostring", ToString);

    RegisterClassTable(state);
}

#include <gtest/gtest.h>

#include "LuaApi/LuaApi.h"

#include "LuaRuntime/LuaProtectedCall.h"
#include "LuaRuntime/LuaState.h"

static void RegisterMathApis(lua_State *L) {
    LuaApi::RegisterVxVector(L);
    LuaApi::RegisterVxColor(L);
    LuaApi::RegisterVxRect(L);
    LuaApi::RegisterVxQuaternion(L);
    LuaApi::RegisterVxMatrix(L);
}

static void RunScript(tas::lua::LuaState &state, const char *script, const char *chunkName) {
    auto load = state.LoadString(script, chunkName);
    ASSERT_TRUE(load.IsOk()) << load.GetError().Format();
    auto call = tas::lua::ProtectedCall(state.Get(), 0, 0);
    ASSERT_TRUE(call.IsOk()) << call.GetError().Format();
}

TEST(LuaApiMathTest, VxVectorOwnedUserdataSupportsFieldsIndexMethodsAndOperators) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    RegisterMathApis(state.Get());

    RunScript(state,
        "local v = VxVector(1, 2, 3)\n"
        "assert(v.x == 1 and v.y == 2 and v.z == 3)\n"
        "v[0], v[1], v[2] = 4, 5, 6\n"
        "local doubled = v + v\n"
        "assert(doubled.x == 8 and doubled.y == 10 and doubled.z == 12)\n"
        "assert(v:dot(VxVector.axis_x()) == 4)\n"
        "local v2 = Vx2DVector(3, 4)\n"
        "assert(v2.magnitude == 5)\n"
        "v2[0], v2[1] = 6, 8\n"
        "assert(v2:dot(Vx2DVector(1, 0)) == 6)\n"
        "local v4 = VxVector4(1, 2, 3, 4)\n"
        "assert(v4.w == 4 and v4[3] == 4)\n"
        "assert((v4 + VxVector4(1, 1, 1, 1)).w == 5)\n",
        "vx_vector_api_test");
}

TEST(LuaApiMathTest, VxColorAndRectExposePackedAndVectorProperties) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    RegisterMathApis(state.Get());

    RunScript(state,
        "local color = VxColor(0.25, 0.5, 0.75, 1.0)\n"
        "assert(color.r == 0.25 and color.g == 0.5 and color.b == 0.75 and color.a == 1.0)\n"
        "color.red = 128\n"
        "assert(type(color.rgba) == 'number' and type(color.rgb) == 'number' and color.red == 128)\n"
        "color:set(0.1, 0.2, 0.3, 0.4)\n"
        "color:check()\n"
        "assert(math.abs(color.a - 0.4) < 0.0001)\n"
        "assert(type(VxColor.convert(1, 1, 1, 1)) == 'number')\n"
        "local rect = VxRect(0, 0, 10, 20)\n"
        "assert(rect.width == 10 and rect.height == 20 and rect.left == 0)\n"
        "rect.center = Vx2DVector(10, 10)\n"
        "assert(rect.center.x == 10 and rect.center.y == 10)\n"
        "rect:set_dimension(1, 2, 3, 4)\n"
        "assert(rect.width == 3 and rect.height == 4)\n"
        "assert(rect:is_empty() == false and rect:is_inside(Vx2DVector(2, 3)) == true)\n",
        "vx_color_rect_api_test");
}

TEST(LuaApiMathTest, VxQuaternionAndMatrixExposeOwnedValueOperations) {
    tas::lua::LuaState state;
    state.OpenStandardLibraries();
    RegisterMathApis(state.Get());

    RunScript(state,
        "local q = VxQuaternion(0, 0, 0, 1)\n"
        "assert(q.w == 1 and q[3] == 1)\n"
        "q[0] = 0.25\n"
        "assert(q.x == 0.25)\n"
        "q:normalize()\n"
        "assert(type(q.magnitude) == 'number' and type(q.conjugate) == 'userdata')\n"
        "local q2 = q * 2\n"
        "assert(type(q:dot(q2)) == 'number' and type(q2:ln()) == 'userdata')\n"
        "local ex, ey, ez = q:to_euler_angles()\n"
        "assert(type(ex) == 'number' and type(ey) == 'number' and type(ez) == 'number')\n"
        "local matrix = VxMatrix.identity()\n"
        "assert(tostring(matrix):find('VxMatrix', 1, true) ~= nil)\n"
        "assert(math.abs(matrix.determinant - 1.0) < 0.001)\n"
        "local matrixProduct = matrix * VxMatrix()\n"
        "assert(matrixProduct == matrix)\n"
        "local mv = matrix:multiply_vector(VxVector(1, 2, 3))\n"
        "assert(mv.x == 1 and mv.y == 2 and mv.z == 3)\n",
        "vx_quaternion_matrix_api_test");
}

#include "LuaApi.h"

#include <VxMath.h>

void LuaApi::RegisterVxQuaternion(sol::state &lua) {
    // ===================================================================
    //  VxQuaternion Registration
    // ===================================================================
    auto quatType = lua.new_usertype<VxQuaternion>(
        "VxQuaternion",
        sol::constructors<VxQuaternion(), VxQuaternion(const VxVector&, float), VxQuaternion(float, float, float, float)>(),

        // Members as properties
        "x", &VxQuaternion::x,
        "y", &VxQuaternion::y,
        "z", &VxQuaternion::z,
        "w", &VxQuaternion::w,

        // Computed properties
        "magnitude", sol::property([](const VxQuaternion &q) { return Magnitude(q); }),
        "conjugate", sol::property([](const VxQuaternion &q) { return Vx3DQuaternionConjugate(q); }),
        "matrix", sol::property([](const VxQuaternion &q) { VxMatrix m; q.ToMatrix(m); return m; }),
        "euler_angles", sol::property([](const VxQuaternion &q) {
            float x, y, z;
            q.ToEulerAngles(&x, &y, &z);
            return std::make_tuple(x, y, z);
        }),

        // Methods
        "from_matrix", [](VxQuaternion &q, const VxMatrix &m, bool is_unit, bool restore) { q.FromMatrix(m, is_unit, restore); },
        "to_matrix", [](const VxQuaternion &q) { VxMatrix m; q.ToMatrix(m); return m; },
        "multiply", &VxQuaternion::Multiply,
        "from_rotation", &VxQuaternion::FromRotation,
        "from_euler_angles", &VxQuaternion::FromEulerAngles,
        "to_euler_angles", [](const VxQuaternion &q) {
            float x, y, z;
            q.ToEulerAngles(&x, &y, &z);
            return std::make_tuple(x, y, z);
        },
        "normalize", &VxQuaternion::Normalize,
        "dot", [](const VxQuaternion &a, const VxQuaternion &b) { return DotProduct(a, b); },
        "slerp", [](const VxQuaternion &a, const VxQuaternion &b, float t) { return Slerp(t, a, b); },
        "squad", [](const VxQuaternion &q1, const VxQuaternion &q1out, const VxQuaternion &q2in, const VxQuaternion &q2, float t) {
            return Squad(t, q1, q1out, q2in, q2);
        },
        "ln", [](const VxQuaternion &q) { return Ln(q); },
        "exp", [](const VxQuaternion &q) { return Exp(q); },
        "ln_dif", [](const VxQuaternion &p, const VxQuaternion &q) { return LnDif(p, q); },

        // Static methods
        "from_matrix_static", [](const VxMatrix &m) { return Vx3DQuaternionFromMatrix(m); },
        "snuggle", [](VxQuaternion &q, VxVector &scale) { return Vx3DQuaternionSnuggle(&q, &scale); },

        // Operators
        sol::meta_function::index, [](const VxQuaternion &q, int i) -> float {
            if (i < 0 || i > 3) throw sol::error("VxQuaternion index out of range [0-3]");
            return q[i];
        },
        sol::meta_function::new_index, [](VxQuaternion &q, int i, float val) {
            if (i < 0 || i > 3) throw sol::error("VxQuaternion index out of range [0-3]");
            q[i] = val;
        },
        sol::meta_function::addition, [](const VxQuaternion &a, const VxQuaternion &b) { return a + b; },
        sol::meta_function::subtraction, [](const VxQuaternion &a, const VxQuaternion &b) { return a - b; },
        sol::meta_function::multiplication,
        sol::overload(
            [](const VxQuaternion &a, const VxQuaternion &b) { return a * b; },
            [](const VxQuaternion &q, float s) { return q * s; },
            [](float s, const VxQuaternion &q) { return s * q; }
        ),
        sol::meta_function::division, [](const VxQuaternion &a, const VxQuaternion &b) { return a / b; },
        sol::meta_function::unary_minus, [](const VxQuaternion &q) { return -q; },
        sol::meta_function::equal_to, [](const VxQuaternion &a, const VxQuaternion &b) { return a == b; },
        sol::meta_function::to_string, [](const VxQuaternion &q) {
            return "VxQuaternion(" + std::to_string(q.x) + ", " + std::to_string(q.y) +
                   ", " + std::to_string(q.z) + ", " + std::to_string(q.w) + ")";
        }
    );
}
#include "LuaApi.h"

#include <VxMath.h>

void LuaApi::RegisterVxVector(sol::state &lua) {
    // ===================================================================
    //  VxVector (Vec3) Registration
    // ===================================================================
    auto vec3Type = lua.new_usertype<VxVector>(
        "VxVector",
        sol::constructors<VxVector(), VxVector(float), VxVector(float, float, float), VxVector(const float [3])>(),

        // Members as properties
        "x", &VxVector::x,
        "y", &VxVector::y,
        "z", &VxVector::z,

        // Computed properties
        "magnitude", sol::readonly_property([](const VxVector &v) { return v.Magnitude(); }),
        "square_magnitude", sol::readonly_property([](const VxVector &v) { return v.SquareMagnitude(); }),
        "inv_magnitude", sol::readonly_property([](const VxVector &v) { return InvMagnitude(v); }),
        "inv_square_magnitude", sol::readonly_property([](const VxVector &v) { return InvSquareMagnitude(v); }),
        "min_component", sol::readonly_property([](const VxVector &v) { return Min(v); }),
        "max_component", sol::readonly_property([](const VxVector &v) { return Max(v); }),
        "absolute", sol::readonly_property([](const VxVector &v) { return Absolute(v); }),

        // Instance Methods
        "normalize", &VxVector::Normalize,
        "set", &VxVector::Set,
        "dot", &VxVector::Dot,
        "rotate", &VxVector::Rotate,
        "cross", [](const VxVector &a, const VxVector &b) { return CrossProduct(a, b); },
        "reflect", [](const VxVector &v, const VxVector &normal) { return Reflect(v, normal); },
        "interpolate", [](const VxVector &a, const VxVector &b, float step) { return Interpolate(step, a, b); },

        // Static axis functions
        "axis_x", &VxVector::axisX,
        "axis_y", &VxVector::axisY,
        "axis_z", &VxVector::axisZ,
        "axis_0", &VxVector::axis0,
        "axis_1", &VxVector::axis1,

        // Static utility functions
        "minimize", [](const VxVector &a, const VxVector &b) { return Minimize(a, b); },
        "maximize", [](const VxVector &a, const VxVector &b) { return Maximize(a, b); },
        "normalize_safe", [](const VxVector &v) { return Normalize(v); },
        "rotate_by_axis", [](const VxVector &v1, const VxVector &v2, float angle) { return Rotate(v1, v2, angle); },

        // Operators
        sol::meta_function::index, [](const VxVector &v, int i) -> float {
            if (i < 0 || i > 2) throw sol::error("VxVector index out of range [0-2]");
            return v[i];
        },
        sol::meta_function::new_index, [](VxVector &v, int i, float val) {
            if (i < 0 || i > 2) throw sol::error("VxVector index out of range [0-2]");
            v[i] = val;
        },
        sol::meta_function::addition,
        sol::overload(
            [](const VxVector &a, const VxVector &b) { return a + b; },
            [](const VxVector &v, float s) { return v + s; }
        ),
        sol::meta_function::subtraction,
        sol::overload(
            [](const VxVector &a, const VxVector &b) { return a - b; },
            [](const VxVector &v, float s) { return v - s; }
        ),
        sol::meta_function::multiplication,
        sol::overload(
            [](const VxVector &v, float s) { return v * s; },
            [](float s, const VxVector &v) { return s * v; },
            [](const VxVector &a, const VxVector &b) { return a * b; }
        ),
        sol::meta_function::division,
        sol::overload(
            [](const VxVector &v, float s) { return v / s; },
            [](const VxVector &a, const VxVector &b) { return a / b; }
        ),
        sol::meta_function::unary_minus, [](const VxVector &v) { return -v; },
        sol::meta_function::equal_to, [](const VxVector &a, const VxVector &b) { return a == b; },
        sol::meta_function::less_than, [](const VxVector &a, const VxVector &b) { return a < b; },
        sol::meta_function::less_than_or_equal_to, [](const VxVector &a, const VxVector &b) { return a <= b; },
        sol::meta_function::to_string, [](const VxVector &v) {
            return "VxVector(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        }
    );

    // ===================================================================
    //  VxVector4 Registration
    // ===================================================================
    auto vec4Type = lua.new_usertype<VxVector4>(
        "VxVector4",
        sol::constructors<VxVector4(), VxVector4(float), VxVector4(float, float, float, float), VxVector4(const float [4])>(),
        sol::base_classes, sol::bases<VxVector>(),

        // Members as properties
        "x", &VxVector4::x,
        "y", &VxVector4::y,
        "z", &VxVector4::z,
        "w", &VxVector4::w,

        // Computed properties
        "magnitude", sol::property([](const VxVector4 &v) {
            return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
        }),
        "square_magnitude", sol::property([](const VxVector4 &v) {
            return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
        }),

        // Methods
        "set", sol::overload(
            [](VxVector4 &v, float x, float y, float z, float w) { v.Set(x, y, z, w); },
            [](VxVector4 &v, float x, float y, float z) { v.Set(x, y, z); }
        ),
        "dot", &VxVector4::Dot,
        "normalize", [](VxVector4 &v) {
            float mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
            if (mag > 0.0f) {
                v.x /= mag;
                v.y /= mag;
                v.z /= mag;
                v.w /= mag;
            }
            return v;
        },

        // Operators
        sol::meta_function::index, [](const VxVector4 &v, int i) -> float {
            if (i < 0 || i > 3) throw sol::error("VxVector4 index out of range [0-3]");
            return v[i];
        },
        sol::meta_function::new_index, [](VxVector4 &v, int i, float val) {
            if (i < 0 || i > 3) throw sol::error("VxVector4 index out of range [0-3]");
            v[i] = val;
        },
        sol::meta_function::addition,
        sol::overload(
            [](const VxVector4 &a, const VxVector4 &b) { return a + b; },
            [](const VxVector4 &v, float s) { return v + s; }
        ),
        sol::meta_function::subtraction,
        sol::overload(
            [](const VxVector4 &a, const VxVector4 &b) { return a - b; },
            [](const VxVector4 &v, float s) { return v - s; }
        ),
        sol::meta_function::multiplication,
        sol::overload(
            [](const VxVector4 &v, float s) { return v * s; },
            [](float s, const VxVector4 &v) { return s * v; },
            [](const VxVector4 &a, const VxVector4 &b) { return a * b; }
        ),
        sol::meta_function::division,
        sol::overload(
            [](const VxVector4 &v, float s) { return v / s; },
            [](const VxVector4 &a, const VxVector4 &b) { return a / b; }
        ),
        sol::meta_function::unary_minus, [](const VxVector4 &v) { return -v; },
        sol::meta_function::equal_to, [](const VxVector4 &a, const VxVector4 &b) { return a == b; },
        sol::meta_function::to_string, [](const VxVector4 &v) {
            return "VxVector4(" + std::to_string(v.x) + ", " + std::to_string(v.y) +
                ", " + std::to_string(v.z) + ", " + std::to_string(v.w) + ")";
        }
    );

    // ===================================================================
    //  Vx2DVector Registration
    // ===================================================================
    auto vec2Type = lua.new_usertype<Vx2DVector>(
        "Vx2DVector",
        sol::constructors<Vx2DVector(), Vx2DVector(float), Vx2DVector(float, float), Vx2DVector(int, int), Vx2DVector(const float [2])>(),

        // Members as properties
        "x", &Vx2DVector::x,
        "y", &Vx2DVector::y,

        // Computed properties
        "magnitude", sol::property([](const Vx2DVector &v) { return v.Magnitude(); }),
        "square_magnitude", sol::property([](const Vx2DVector &v) { return v.SquareMagnitude(); }),
        "min_component", sol::property([](const Vx2DVector &v) { return v.Min(); }),
        "max_component", sol::property([](const Vx2DVector &v) { return v.Max(); }),

        // Methods
        "normalize", &Vx2DVector::Normalize,
        "set", sol::overload(
            [](Vx2DVector &v, float x, float y) { v.Set(x, y); },
            [](Vx2DVector &v, int x, int y) { v.Set(x, y); }
        ),
        "dot", &Vx2DVector::Dot,
        "cross", &Vx2DVector::Cross,

        // Operators
        sol::meta_function::index, [](const Vx2DVector &v, int i) -> float {
            if (i < 0 || i > 1) throw sol::error("Vx2DVector index out of range [0-1]");
            return v[i];
        },
        sol::meta_function::new_index, [](Vx2DVector &v, int i, float val) {
            if (i < 0 || i > 1) throw sol::error("Vx2DVector index out of range [0-1]");
            v[i] = val;
        },
        sol::meta_function::addition, [](const Vx2DVector &a, const Vx2DVector &b) { return a + b; },
        sol::meta_function::subtraction, [](const Vx2DVector &a, const Vx2DVector &b) { return a - b; },
        sol::meta_function::multiplication,
        sol::overload(
            [](const Vx2DVector &v, float s) { return v * s; },
            [](float s, const Vx2DVector &v) { return s * v; },
            [](const Vx2DVector &a, const Vx2DVector &b) { return a * b; }
        ),
        sol::meta_function::division,
        sol::overload(
            [](const Vx2DVector &v, float s) { return v / s; },
            [](float s, const Vx2DVector &v) { return s / v; },
            [](const Vx2DVector &a, const Vx2DVector &b) { return a / b; }
        ),
        sol::meta_function::unary_minus, [](const Vx2DVector &v) { return -v; },
        sol::meta_function::equal_to, [](const Vx2DVector &a, const Vx2DVector &b) { return a == b; },
        sol::meta_function::less_than, [](const Vx2DVector &a, const Vx2DVector &b) { return a < b; },
        sol::meta_function::less_than_or_equal_to, [](const Vx2DVector &a, const Vx2DVector &b) { return a <= b; },
        sol::meta_function::to_string, [](const Vx2DVector &v) {
            return "Vx2DVector(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        }
    );

    // ===================================================================
    //  VxBbox Registration
    // ===================================================================
    auto bboxType = lua.new_usertype<VxBbox>(
        "VxBbox",
        sol::constructors<VxBbox(), VxBbox(VxVector, VxVector), VxBbox(float)>(),

        // Members as properties
        "max", &VxBbox::Max,
        "min", &VxBbox::Min,

        // Computed properties
        "size", sol::property([](const VxBbox &b) { return b.GetSize(); }),
        "half_size", sol::property([](const VxBbox &b) { return b.GetHalfSize(); }),
        "center", sol::property(
            [](const VxBbox &b) { return b.GetCenter(); },
            [](VxBbox &b, const VxVector &center) {
                VxVector halfSize = b.GetHalfSize();
                b.SetCenter(center, halfSize);
            }
        ),

        // Methods
        "is_valid", &VxBbox::IsValid,
        "set_corners", &VxBbox::SetCorners,
        "set_dimension", &VxBbox::SetDimension,
        "set_center", &VxBbox::SetCenter,
        "reset", &VxBbox::Reset,
        "merge", sol::overload(
            [](VxBbox &b, const VxBbox &other) { b.Merge(other); },
            [](VxBbox &b, const VxVector &v) { b.Merge(v); }
        ),
        "classify", sol::overload(
            [](const VxBbox &b, const VxVector &pt) { return b.Classify(pt); },
            [](const VxBbox &b, const VxBbox &other) { return b.Classify(other); },
            [](const VxBbox &b, const VxBbox &other, const VxVector &pt) { return b.Classify(other, pt); }
        ),
        "classify_vertices", &VxBbox::ClassifyVertices,
        "classify_vertices_one_axis", &VxBbox::ClassifyVerticesOneAxis,
        "intersect", &VxBbox::Intersect,
        "vector_in", &VxBbox::VectorIn,
        "is_box_inside", &VxBbox::IsBoxInside,
        "transform_to", &VxBbox::TransformTo,
        "transform_from", &VxBbox::TransformFrom,

        // Operators
        sol::meta_function::equal_to, [](const VxBbox &a, const VxBbox &b) { return a == b; },
        sol::meta_function::to_string, [](const VxBbox &b) {
            return "VxBbox(min=" + std::to_string(b.Min.x) + "," + std::to_string(b.Min.y) + "," + std::to_string(b.Min.z) +
                   ", max=" + std::to_string(b.Max.x) + "," + std::to_string(b.Max.y) + "," + std::to_string(b.Max.z) + ")";
        }
    );

    // ===================================================================
    //  VxCompressedVector Registration
    // ===================================================================
    auto compVecType = lua.new_usertype<VxCompressedVector>(
        "VxCompressedVector",
        sol::constructors<VxCompressedVector(), VxCompressedVector(float, float, float)>(),

        // Members as properties
        "xa", &VxCompressedVector::xa,
        "ya", &VxCompressedVector::ya,

        // Methods
        "set", &VxCompressedVector::Set,
        "slerp", &VxCompressedVector::Slerp,

        sol::meta_function::to_string, [](const VxCompressedVector &v) {
            return "VxCompressedVector(" + std::to_string(v.xa) + ", " + std::to_string(v.ya) + ")";
        }
    );

    // ===================================================================
    //  VxCompressedVectorOld Registration
    // ===================================================================
    auto compVecOldType = lua.new_usertype<VxCompressedVectorOld>(
        "VxCompressedVectorOld",
        sol::constructors<VxCompressedVectorOld(), VxCompressedVectorOld(float, float, float)>(),

        // Members as properties
        "xa", &VxCompressedVectorOld::xa,
        "ya", &VxCompressedVectorOld::ya,

        // Methods
        "set", &VxCompressedVectorOld::Set,
        "slerp", &VxCompressedVectorOld::Slerp,

        sol::meta_function::to_string, [](const VxCompressedVectorOld &v) {
            return "VxCompressedVectorOld(" + std::to_string(v.xa) + ", " + std::to_string(v.ya) + ")";
        }
    );
}

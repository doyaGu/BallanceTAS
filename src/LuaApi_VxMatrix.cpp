#include "LuaApi.h"

#include <VxMath.h>

void LuaApi::RegisterVxMatrix(sol::state &lua) {
    // ===================================================================
    //  VxMatrix Registration
    // ===================================================================
    auto matrixType = lua.new_usertype<VxMatrix>(
        "VxMatrix",
        sol::constructors<VxMatrix()>(),

        // Computed properties
        "determinant", sol::readonly_property([](const VxMatrix &m) { return Vx3DMatrixDeterminant(m); }),
        "inverse", sol::readonly_property([](const VxMatrix &m) {
            VxMatrix result;
            Vx3DInverseMatrix(result, m);
            return result;
        }),
        "transpose", sol::readonly_property([](const VxMatrix &m) {
            VxMatrix result;
            Vx3DTransposeMatrix(result, m);
            return result;
        }),

        // Methods
        "clear", &VxMatrix::Clear,
        "set_identity", &VxMatrix::SetIdentity,
        "orthographic", &VxMatrix::Orthographic,
        "perspective", &VxMatrix::Perspective,
        "orthographic_rect", &VxMatrix::OrthographicRect,
        "perspective_rect", &VxMatrix::PerspectiveRect,
        "multiply", [](const VxMatrix &a, const VxMatrix &b) {
            VxMatrix result;
            Vx3DMultiplyMatrix(result, a, b);
            return result;
        },
        "multiply_vector", [](const VxMatrix &m, const VxVector &v) {
            VxVector result;
            Vx3DMultiplyMatrixVector(&result, m, &v);
            return result;
        },
        "multiply_vector4", [](const VxMatrix &m, const VxVector4 &v) {
            VxVector4 result;
            Vx3DMultiplyMatrixVector4(&result, m, &v);
            return result;
        },
        "rotate_vector", [](const VxMatrix &m, const VxVector &v) {
            VxVector result;
            Vx3DRotateVector(&result, m, &v);
            return result;
        },
        "to_euler_angles", [](const VxMatrix &m) {
            float x, y, z;
            Vx3DMatrixToEulerAngles(m, &x, &y, &z);
            return std::make_tuple(x, y, z);
        },
        "interpolate", [](const VxMatrix &a, const VxMatrix &b, float step) {
            VxMatrix result;
            Vx3DInterpolateMatrix(step, result, a, b);
            return result;
        },
        "interpolate_no_scale", [](const VxMatrix &a, const VxMatrix &b, float step) {
            VxMatrix result;
            Vx3DInterpolateMatrixNoScale(step, result, a, b);
            return result;
        },
        "decompose", [](const VxMatrix &m) {
            VxQuaternion quat;
            VxVector pos, scale;
            Vx3DDecomposeMatrix(m, quat, pos, scale);
            return std::make_tuple(quat, pos, scale);
        },

        // Static construction methods
        "identity", &VxMatrix::Identity,
        "from_rotation", [](const VxVector &axis, float angle) {
            VxMatrix result;
            Vx3DMatrixFromRotation(result, axis, angle);
            return result;
        },
        "from_rotation_and_origin", [](const VxVector &axis, const VxVector &origin, float angle) {
            VxMatrix result;
            Vx3DMatrixFromRotationAndOrigin(result, axis, origin, angle);
            return result;
        },
        "from_euler_angles", [](float x, float y, float z) {
            VxMatrix result;
            Vx3DMatrixFromEulerAngles(result, x, y, z);
            return result;
        },

        // Operators
        sol::meta_function::multiplication, [](const VxMatrix &a, const VxMatrix &b) { return a * b; },
        sol::meta_function::equal_to, [](const VxMatrix &a, const VxMatrix &b) { return a == b; },
        sol::meta_function::to_string, [](const VxMatrix &m) {
            return "VxMatrix(" +
                   std::to_string(m[0][0]) + ", " + std::to_string(m[0][1]) + ", " + std::to_string(m[0][2]) +
                   ", " + std::to_string(m[0][3]) + ", " + std::to_string(m[1][0]) + ", " +
                   std::to_string(m[1][1]) + ", " + std::to_string(m[1][2]) + ", " + std::to_string(m[1][3]) +
                   ", " + std::to_string(m[2][0]) + ", " + std::to_string(m[2][1]) + ", " +
                   std::to_string(m[2][2]) + ", " + std::to_string(m[2][3]) + ", " +
                   std::to_string(m[3][0]) + ", " + std::to_string(m[3][1]) + ", " +
                   std::to_string(m[3][2]) + ", " + std::to_string(m[3][3]) + ")";
        }
    );
}

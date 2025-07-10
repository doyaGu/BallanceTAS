#include "LuaApi.h"

#include "physics_RT.h"

#include <VxVector.h>
#include <VxMatrix.h>

void LuaApi::RegisterPhysicsObject(sol::state &lua) {
    // ===================================================================
    //  PhysicsObject - Physics simulation object
    // ===================================================================
    auto physicsObjectType = lua.new_usertype<PhysicsObject>(
        "PhysicsObject",
        sol::no_constructor, // Objects are likely created by the physics system

        // Basic properties
        "name", sol::property(&PhysicsObject::GetName),
        "entity", sol::property(&PhysicsObject::GetEntity),

        // Physics state
        "wake", &PhysicsObject::Wake,
        "is_static", &PhysicsObject::IsStatic,

        // Mass properties
        "mass", sol::property(&PhysicsObject::GetMass),
        "inv_mass", sol::property(&PhysicsObject::GetInvMass),

        // Inertia properties
        "get_inertia", [](PhysicsObject &obj) {
            VxVector inertia;
            obj.GetInertia(inertia);
            return inertia;
        },
        "get_inv_inertia", [](PhysicsObject &obj) {
            VxVector inertia;
            obj.GetInvInertia(inertia);
            return inertia;
        },

        // Damping properties
        "get_damping_speed", [](PhysicsObject &obj) {
            float speed;
            obj.GetDamping(&speed, nullptr);
            return speed;
        },
        "get_damping_rotation", [](PhysicsObject &obj) {
            float rot;
            obj.GetDamping(nullptr, &rot);
            return rot;
        },

        // Position and orientation
        "get_position", [](PhysicsObject &obj) {
            VxVector position;
            obj.GetPosition(&position, nullptr);
            return position;
        },
        "get_orientation", [](PhysicsObject &obj) {
            VxVector orientation;
            obj.GetPosition(nullptr, &orientation);
            return orientation;
        },

        // Position matrix
        "get_position_matrix", [](PhysicsObject &obj) {
            VxMatrix matrix;
            obj.GetPositionMatrix(matrix);
            return matrix;
        },

        // Velocity
        "get_linear_velocity", [](PhysicsObject &obj) {
            VxVector velocity;
            obj.GetVelocity(&velocity, nullptr);
            return velocity;
        },
        "get_angular_velocity", [](PhysicsObject &obj) {
            VxVector angularVelocity;
            obj.GetVelocity(nullptr, &angularVelocity);
            return angularVelocity;
        },

        // Set velocity (support multiple call patterns)
        "set_velocity", sol::overload(
            // Set both linear and angular velocity
            [](PhysicsObject &obj, const VxVector &linear, const VxVector &angular) {
                obj.SetVelocity(&linear, &angular);
            },
            // Set only linear velocity
            [](PhysicsObject &obj, const VxVector &linear) {
                obj.SetVelocity(&linear, nullptr);
            }
        ),

        "set_linear_velocity", [](PhysicsObject &obj, const VxVector &linear) {
            obj.SetVelocity(&linear, nullptr);
        },

        "set_angular_velocity", [](PhysicsObject &obj, const VxVector &angular) {
            obj.SetVelocity(nullptr, &angular);
        }
    );
}
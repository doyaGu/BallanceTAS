#pragma once

/**
 * @file IPhysicsProvider.h
 * @brief Interface for physics-related game queries.
 *
 * Consumers that only need physics data (positions, velocities, floors) depend
 * on this narrow interface instead of the full GameInterface.
 */

// Forward declarations for Virtools types used in the interface
class CK3dEntity;
struct VxVector;
struct VxQuaternion;
class XObjectArray;
struct PhysicsObject;

class IPhysicsProvider {
public:
    virtual ~IPhysicsProvider() = default;

    virtual VxVector GetPosition(CK3dEntity *obj) const = 0;
    virtual VxQuaternion GetRotation(CK3dEntity *obj) const = 0;
    virtual VxVector GetVelocity(CK3dEntity *obj) const = 0;
    virtual VxVector GetAngularVelocity(CK3dEntity *obj) const = 0;
    virtual bool IsSleeping(CK3dEntity *obj) const = 0;
    virtual XObjectArray GetFloors(CK3dEntity *ent, float zoom = 2.0f, float maxHeight = 100.0f) const = 0;

    virtual void ResetPhysicsTime() = 0;
    virtual void SetPhysicsTimeFactor(float factor = 1.0f) = 0;
};

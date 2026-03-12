#pragma once

#include <string>

/**
 * @file IObjectProvider.h
 * @brief Interface for game object access and lookup.
 *
 * Consumers that need to find or manipulate game objects depend on this narrow
 * interface instead of the full GameInterface.
 */

class CK3dEntity;
class CKCamera;
class CKParameter;
struct PhysicsObject;

class IObjectProvider {
public:
    virtual ~IObjectProvider() = default;

    virtual CK3dEntity *GetActiveBall() const = 0;
    virtual void SetActiveBall(CKParameter *param) = 0;
    virtual CKCamera *GetActiveCamera() const = 0;
    virtual CK3dEntity *GetObjectByName(const std::string &name) const = 0;
    virtual CK3dEntity *GetObjectByID(int id) const = 0;
    virtual PhysicsObject *GetPhysicsObject(CK3dEntity *entity) const = 0;
};

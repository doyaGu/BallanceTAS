#pragma once

#include <CKAll.h>
#include <sol/sol.hpp>

#include <BML/ILogger.h>
#include <BML/InputHook.h>

#include "physics_RT.h"
#include "UIManager.h"

// Forward declarations
class BallanceTAS;
class CKIpionManager;
class UIManager;
class IBML;

/**
 * @class GameInterface
 * @brief Provides an interface to access game objects and their properties in Ballance.
 *
 * This class serves as the backend for all game object queries and manipulations.
 */
class GameInterface {
public:
    explicit GameInterface(BallanceTAS *mod);

    // GameInterface is not copyable or movable
    GameInterface(const GameInterface &) = delete;
    GameInterface &operator=(const GameInterface &) = delete;

    ILogger *GetLogger() const;
    void AddTimer(size_t tick, const std::function<void()> &callback);

    CKContext *GetCKContext() const { return m_CKContext; }
    CKRenderContext *GetRenderContext() const { return m_RenderContext; }
    CKTimeManager *GetTimeManager() const { return m_TimeManager; }
    InputHook *GetInputManager() const { return m_InputManager; }
    CKIpionManager *GetIpionManager() const { return m_IpionManager; }
    UIManager *GetUIManager() const;

    void SetUIMode(UIMode mode);

    void AcquireGameplayInfo();
    void AcquireKeyBindings();

    void SetMapName(const std::string &name) { m_MapName = name; }
    const std::string &GetMapName() const { return m_MapName; }

    void ResetPhysicsTime();
    void SetPhysicsTimeFactor(float factor = 1.0f);

    CKKEYBOARD RemapKey(CKKEYBOARD key) const;

    // --- Object Access ---

    /**
     * @brief Gets the CK3dEntity for the currently controlled ball.
     * @return A pointer to the ball's entity, or nullptr if not found.
     */
    CK3dEntity *GetActiveBall() const;

    /**
     * @brief Sets the active ball parameter.
     * @param param The CKParameter that holds the active ball entity.
     */
    void SetActiveBall(CKParameter *param);

    /**
     * @brief Gets the active camera in the scene.
     * @return A pointer to the active CKCamera, or nullptr if not found.
     */
    CKCamera *GetActiveCamera() const;
    /**
     * @brief Gets a game object by its name.
     * @param name The name of the CK3dEntity.
     * @return A pointer to the entity, or nullptr if not found.
     */
    CK3dEntity *GetObjectByName(const std::string &name) const;

    /**
     * @brief Gets a game object by its id.
     * @param id The id of the CK3dEntity.
     * @return A pointer to the entity, or nullptr if not found.
     */
    CK3dEntity *GetObjectByID(int id) const;

    /**
     * @brief Gets the PhysicsObject for a given CK3dEntity.
     * @param entity A pointer to the CK3dEntity.
     * @return A pointer to the PhysicsObject, or nullptr if not found.
     */
    PhysicsObject *GetPhysicsObject(CK3dEntity *entity) const;

    // --- Object Property Queries ---

    /**
     * @brief Gets the world position of a game entity.
     * @param obj A pointer to the CK3dEntity.
     * @return The entity's position as a VxVector. Returns a zero vector if obj is null.
     */
    VxVector GetPosition(CK3dEntity *obj) const;

    /**
     * @brief Gets the world rotation of a game entity.
     * @param obj A pointer to the CK3dEntity.
     * @return The entity's rotation as a VxQuaternion. Returns an identity quaternion if obj is null.
     */
    VxQuaternion GetRotation(CK3dEntity *obj) const;

    /**
     * @brief Gets the world velocity of a game entity.
     * @param obj A pointer to the CK3dEntity.
     * @return The entity's linear velocity as a VxVector. Returns a zero vector if obj is null.
     */
    VxVector GetVelocity(CK3dEntity *obj) const;

    /**
     * @brief Gets the angular velocity of a game entity.
     * @param obj A pointer to the CK3dEntity.
     * @return The entity's angular velocity. Returns a zero vector if obj is null.
     */
    VxVector GetAngularVelocity(CK3dEntity *obj) const;

    /**
     * @brief Checks if a physics object is in a sleeping state.
     * @param obj A pointer to the CK3dEntity.
     * @return True if the object is sleeping, false otherwise.
     */
    bool IsSleeping(CK3dEntity *obj) const;

    // --- Gameplay State Queries ---

    bool IsLegacyMode() const;

    bool IsIngame() const;
    bool IsPaused() const;
    bool IsPlaying() const;

    float GetSRScore() const;
    int GetHSScore() const;

    int GetPoints() const;

    int GetLifeCount() const;

    int GetCurrentLevel() const;

    int GetCurrentSector() const;

    XObjectArray GetFloors(CK3dEntity *ent, float zoom = 2.0f, float maxHeight = 100.0f) const;

    void PrintMessage(const char *message) const;

    void SkipRenderForTicks(size_t ticks);

    void OnCloseMenu();

private:
    BallanceTAS *m_Mod = nullptr;
    IBML *m_BML = nullptr;

    CKContext *m_CKContext = nullptr;
    CKRenderContext *m_RenderContext = nullptr;
    CKTimeManager *m_TimeManager = nullptr;
    InputHook *m_InputManager = nullptr;
    CKIpionManager *m_IpionManager = nullptr;

    std::string m_MapName;

    CKDataArray *m_Keyboard = nullptr;
    CKDataArray *m_CurrentLevel = nullptr;
    CKDataArray *m_Energy = nullptr;
    CKDataArray *m_CheckPoints = nullptr;
    CKDataArray *m_IngameParam = nullptr;
    CKParameter *m_CurrentSector = nullptr;
    CKParameter *m_ActiveBallParam = nullptr;

    CK2dEntity *m_Level01 = nullptr;
    CKBehavior *m_ExitStart = nullptr;
    CKBehavior *m_ExitMain = nullptr;

    sol::state *m_LuaState = nullptr;
};

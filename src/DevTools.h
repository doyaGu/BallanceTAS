#pragma once

#include <BML/BMLAll.h>

// Forward declarations
class CK3dEntity;
struct VxVector;
class CKIpionManager;

/**
 * @class DevTools
 * @brief Provides a set of "developer" or "cheat" tools for debugging and script creation.
 *
 * This class encapsulates all functionality that can break the determinism of a TAS
 * playback, such as teleporting objects or changing the game speed. Access to these
 * tools is gated and should only be enabled for debugging purposes.
 */
class DevTools {
public:
    explicit DevTools(IBML *bml);

    // --- Control ---

    /**
     * @brief Enables or disables the developer tools.
     * When disabled, all methods will have no effect.
     * @param enabled The new enabled state.
     */
    void SetEnabled(bool enabled);

    /**
     * @brief Checks if the developer tools are currently enabled.
     * @return True if enabled, false otherwise.
     */
    bool IsEnabled() const { return m_Enabled; }

    // --- Tool Functions ---

    /**
     * @brief Sets the game's time scale.
     * @param factor 1.0 is normal speed, 0.5 is half speed, etc.
     */
    void SetTimeScale(float factor);

    // /**
    //  * @brief Instantly moves a game object to a new position.
    //  * @param obj The CK3dEntity to teleport.
    //  * @param position The target world position.
    //  */
    // void Teleport(CK3dEntity* obj, const VxVector& position);

    /**
     * @brief Instantly sets a game object's velocity.
     * @param obj The CK3dEntity to modify.
     * @param velocity The new linear velocity.
     */
    void SetVelocity(CK3dEntity *obj, const VxVector &velocity);

    /**
     * @brief Skips rendering for the next frame.
     */
    void SkipRenderForNextTick();

private:
    IBML *m_BML;
    CKIpionManager *m_IpionManager;
    bool m_Enabled = false;
};

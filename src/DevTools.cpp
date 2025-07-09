#include "DevTools.h"

#include <CK3dEntity.h>
#include <CKContext.h>

#include "physics_RT.h"

DevTools::DevTools(IBML *bml) : m_BML(bml), m_IpionManager(nullptr) {
    if (m_BML) {
        CKContext *context = m_BML->GetCKContext();
        if (context) {
            m_IpionManager = static_cast<CKIpionManager *>(context->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148)));
        }
    }
}

void DevTools::SetEnabled(bool enabled) {
    m_Enabled = enabled;
}

void DevTools::SetTimeScale(float factor) {
    // We don't check IsEnabled() here because this function is also used
    // internally by the ErrorModal to pause the game, even in normal mode.

    if (factor <= 0) factor = 0.0001f; // Prevent division by zero or negative time.

    auto &physicsTimeFactor = *reinterpret_cast<float *>(reinterpret_cast<CKBYTE *>(m_IpionManager) + 0xD0);
    physicsTimeFactor = factor * 0.001f;
}

// void DevTools::Teleport(CK3dEntity *obj, const VxVector &position) {
//     if (!m_Enabled || !obj || !m_IpionManager) return;
//
//     PhysicsObject *physObj = m_IpionManager->GetPhysicsObject(obj);
//     if (physObj) {
//         // The `SetPosition` method on IPhysicsObject handles teleporting.
//         // The `isTeleport` flag is crucial to ensure the physics engine
//         // correctly resets any interpolation/state data.
//         // physObj->SetPosition(position, VxVector(0, 0, 0), true);
//         physObj->Wake(); // Ensure the object is awake after teleporting
//     }
// }

void DevTools::SetVelocity(CK3dEntity *obj, const VxVector &velocity) {
    if (!m_Enabled || !obj || !m_IpionManager) return;

    PhysicsObject *physObj = m_IpionManager->GetPhysicsObject(obj);
    if (physObj) {
        // Set both linear and angular velocity. We set angular to zero
        // unless a separate function is provided for it.
        physObj->SetVelocity(&velocity, nullptr);
        physObj->Wake(); // Ensure the object is awake after changing velocity
    }
}

void DevTools::SkipRenderForNextTick() {
    m_BML->SkipRenderForNextTick();
}

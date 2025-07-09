#include "GameInterface.h"

#include "physics_RT.h"
#include "BallanceTAS.h"

GameInterface::GameInterface(BallanceTAS *mod) : m_Mod(mod) {
    if (!m_Mod) return;

    auto *bml = m_Mod->GetBML();

    // Cache pointers to frequently accessed managers and parameters for performance.
    CKContext *context = bml->GetCKContext();
    if (!context) return;

    m_TimeManager = context->GetTimeManager();
    m_IpionManager = static_cast<CKIpionManager *>(context->GetManagerByGuid(CKGUID(0x6bed328b, 0x141f5148)));
}

void GameInterface::AcquireGameplayInfo() {
    auto *bml = m_Mod->GetBML();

    // 3D Entities\Gameplay.nmo
    m_CurrentLevel = bml->GetArrayByName("CurrentLevel");
    m_CheckPoints = bml->GetArrayByName("Checkpoints");
    m_IngameParam = bml->GetArrayByName("IngameParameter");
    CKBehavior *events = bml->GetScriptByName("Gameplay_Events");
    CKBehavior *id = ScriptHelper::FindNextBB(events, events->GetInput(0));
    m_CurrentSector = id->GetOutputParameter(0)->GetDestination(0);
}

void GameInterface::AcquireKeyBindings() {
    auto *bml = m_Mod->GetBML();

    m_Keyboard = bml->GetArrayByName("Keyboard");
    if (m_Keyboard) {
        m_Keyboard->GetElementValue(0, 0, &m_KeyUp);
        m_Keyboard->GetElementValue(0, 1, &m_KeyDown);
        m_Keyboard->GetElementValue(0, 2, &m_KeyLeft);
        m_Keyboard->GetElementValue(0, 3, &m_KeyRight);
        m_Keyboard->GetElementValue(0, 4, &m_KeyShift);
        m_Keyboard->GetElementValue(0, 5, &m_KeySpace);
    }
}

void GameInterface::SetActiveBall(CKParameter *param) {
    m_ActiveBallParam = param;
}

CKCamera *GameInterface::GetActiveCamera() {
    return m_Mod->GetBML()->GetRenderContext()->GetAttachedCamera();
}

void GameInterface::ResetPhysicsTime() {
    // Reset physics time in order to sync with TAS records
    // IVP_Environment
    auto *env = *reinterpret_cast<CKBYTE **>(reinterpret_cast<CKBYTE *>(m_IpionManager) + 0xC0);

    auto &base_time = *reinterpret_cast<double *>(*reinterpret_cast<CKBYTE **>(env + 0x4) + 0x18);
#ifdef _DEBUG
    m_Mod->GetLogger()->Info("time_manager->base_time: %f", base_time);
#endif
    base_time = 0;

    auto &current_time = *reinterpret_cast<double *>(env + 0x120);
#ifdef _DEBUG
    m_Mod->GetLogger()->Info("current_time: %f", current_time);
#endif
    current_time = 0;

    auto &time_of_last_psi = *reinterpret_cast<double *>(env + 0x130);
#ifdef _DEBUG
    m_Mod->GetLogger()->Info("time_of_last_psi: %f", time_of_last_psi);
#endif
    time_of_last_psi = 0;

    auto &time_of_next_psi = *reinterpret_cast<double *>(env + 0x128);
#ifdef _DEBUG
    m_Mod->GetLogger()->Info("time_of_next_psi: %f", time_of_next_psi);
#endif
    time_of_next_psi = time_of_last_psi + 1.0 / 66;

//     auto &current_time_code = *reinterpret_cast<double *>(env + 0x138);
// #ifdef _DEBUG
//     m_Mod->GetLogger()->Info("current_time_code: %f", current_time_code);
// #endif
//     current_time_code = 1;

    auto &deltaTime = *reinterpret_cast<float *>(reinterpret_cast<CKBYTE *>(m_IpionManager) + 0xC8);
    deltaTime = m_TimeManager->GetLastDeltaTime();
}

void GameInterface::SetPhysicsTimeFactor(float factor) {
    // Reset physics time factor in case it was changed
    auto &physicsTimeFactor = *reinterpret_cast<float *>(reinterpret_cast<CKBYTE *>(m_IpionManager) + 0xD0);
    physicsTimeFactor = factor * 0.001f;
}

CKKEYBOARD GameInterface::RemapKey(CKKEYBOARD key) const {
    switch (key) {
        case CKKEY_UP: return m_KeyUp;
        case CKKEY_DOWN: return m_KeyDown;
        case CKKEY_LEFT: return m_KeyLeft;
        case CKKEY_RIGHT: return m_KeyRight;
        case CKKEY_LSHIFT: return m_KeyShift;
        case CKKEY_SPACE: return m_KeySpace;
        default: return key; // Return the original key if not remapped
    }
}

CK3dEntity *GameInterface::GetBall() const {
    if (m_ActiveBallParam) {
        CKObject *obj = m_ActiveBallParam->GetValueObject();
        return static_cast<CK3dEntity *>(obj);
    }
    return nullptr;
}

CK3dEntity *GameInterface::GetObjectByName(const std::string &name) const {
    return m_Mod->GetBML()->Get3dEntityByName(name.c_str());
}

VxVector GameInterface::GetPosition(CK3dEntity *obj) const {
    if (!obj) return VxVector(0, 0, 0);

    VxVector pos;
    obj->GetPosition(&pos);
    return pos;
}

VxVector GameInterface::GetVelocity(CK3dEntity *obj) const {
    if (!obj || !m_IpionManager) return VxVector(0, 0, 0);

    PhysicsObject *physObj = m_IpionManager->GetPhysicsObject(obj);
    if (!physObj) return VxVector(0, 0, 0);

    VxVector vel;
    physObj->GetVelocity(&vel, nullptr);
    return vel;
}

VxQuaternion GameInterface::GetRotation(CK3dEntity *obj) const {
    if (!obj) return VxQuaternion(0, 0, 0, 1);

    VxQuaternion rot;
    obj->GetQuaternion(&rot);
    return rot;
}

VxVector GameInterface::GetAngularVelocity(CK3dEntity *obj) const {
    if (!obj || !m_IpionManager) return VxVector(0, 0, 0);

    PhysicsObject *physObj = m_IpionManager->GetPhysicsObject(obj);
    if (!physObj) return VxVector(0, 0, 0);

    VxVector angVel;
    physObj->GetVelocity(nullptr, &angVel);
    return angVel;
}

bool GameInterface::IsSleeping(CK3dEntity *obj) const {
    if (!obj || !m_IpionManager) return false;

    // This requires access to the underlying physics object.
    // The IPhysicsObject interface in BML might not expose this directly.
    // We may need to use the older `CKIpionManager::GetPhysicsObject` to access the concrete type.
    PhysicsObject *ipionObj = m_IpionManager->GetPhysicsObject(obj);
    if (!ipionObj) return false;

    return ipionObj->m_RealObject->get_core()->movement_state >= IVP_MT_CALM;
}

unsigned int GameInterface::GetCurrentTick() const {
    return m_CurrentTick;
}

void GameInterface::SetCurrentTick(unsigned int tick) {
    m_CurrentTick = tick;
}

int GameInterface::GetCurrentSector() const {
    int sector = -1;
    m_CurrentSector->GetValue(&sector);
    return sector;
}

XObjectArray GameInterface::GetFloors(CK3dEntity *ent, float zoom, float maxHeight) const {
    XObjectArray floors;

    const VxBbox &Bbox_obj = ent->GetBoundingBox();
    float inv_zoom = 1.0f / (zoom * (Bbox_obj.Max.x - Bbox_obj.Min.x));

    CKContext *context = m_Mod->GetBML()->GetCKContext();
    if (!context) return floors;

    auto *FloorManager = (CKFloorManager *) context->GetManagerByGuid(FLOOR_MANAGER_GUID);
    int floorAttribute = FloorManager->GetFloorAttribute();

    CKAttributeManager *attman = context->GetAttributeManager();
    const XObjectPointerArray &floor_objects = attman->GetGlobalAttributeListPtr(floorAttribute);

    VxVector vPos, scale;

    int nbf_under = 0; // number of floors under

    int floor_count = floor_objects.Size();

    for (int n = 0; n < floor_count; ++n) {
        auto *floor = (CK3dEntity *) floor_objects[n];

        if (floor->IsVisible() && !floor->IsAllOutsideFrustrum()) {
            // ... and just the VISIBLE floors
            // rem: IsAllOutsideFrustrum() function will result in a 1 frame delayed appearance
            // if the floor was totally outside the viewing frustum during the previous rendering,
            // and becomes very visible now... (if this is the case, the IsAllOutsideFrustrum()
            // function can be removed ).

            bool under = false;

            const VxBbox &Bbox_floorWorld = floor->GetBoundingBox();

            // test the two world bbox on XZ plane
            if (Bbox_floorWorld.Min.x > Bbox_obj.Max.x ||
                Bbox_floorWorld.Min.z > Bbox_obj.Max.z ||
                Bbox_floorWorld.Max.x < Bbox_obj.Min.x ||
                Bbox_floorWorld.Max.z < Bbox_obj.Min.z)
                continue;

            // test the two bbox on height
            if (Bbox_floorWorld.Min.y > Bbox_obj.Max.y ||           // floor is above the object
                Bbox_obj.Min.y - Bbox_floorWorld.Max.y > maxHeight) // floor is too below
                continue;

            ent->GetPosition(&vPos, floor);
            const VxBbox &Bbox_floor = floor->GetBoundingBox(TRUE);

            floor->GetScale(&scale);

            float tmp_x = inv_zoom * scale.x;
            float tmp_z = inv_zoom * scale.z;

            if ((Bbox_floor.Max.x - vPos.x) * tmp_x >= -0.5f)
                if ((Bbox_floor.Min.x - vPos.x) * tmp_x <= 0.5f)
                    if ((Bbox_floor.Max.z - vPos.z) * tmp_z >= -0.5f)
                        if ((Bbox_floor.Min.z - vPos.z) * tmp_z <= 0.5f) {
                            // floors is UNDER if the UV of its Bbox
                            // are possibly in the texture (0-1)
                            under = true;
                        }

            if (under) {
                // limit to 50 floors
                if (nbf_under < 50) {
                    floors.PushBack(floor->GetID());
                    nbf_under++;
                }
            }
        }
    }

    return floors;
}

void GameInterface::PrintMessage(const char *message) const {
    m_Mod->GetBML()->SendIngameMessage(message);
}

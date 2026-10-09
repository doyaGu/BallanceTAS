#include "PhysicsDeterminism.h"

#include <float.h>

#include "CKBaseManager.h"

#include "Hook.h"
#include "Logger.h"

namespace {
    constexpr unsigned int kFpuMask = _MCW_PC | _MCW_RC;
    constexpr uint32_t kMaxLoggedChanges = 20;

    bool g_Monitoring = false;
    bool g_HasLastSeen = false;
    unsigned int g_LastSeen = 0;
    uint32_t g_Changes = 0;

    unsigned int ReadX87Control() {
        unsigned int x87 = 0;
        __control87_2(0, 0, &x87, nullptr);
        return x87 & kFpuMask;
    }

    class IpionPostProcessHook : public CKBaseManager {
    public:
        using MethodType = decltype(&CKBaseManager::PostProcess);

        CKERROR Detour() {
            if (g_Monitoring) {
                const unsigned int current = ReadX87Control();
                if (!g_HasLastSeen) {
                    g_LastSeen = current;
                    g_HasLastSeen = true;
                    Log::Info("Physics FPU control word at TAS start: 0x%X", current);
                } else if (current != g_LastSeen) {
                    ++g_Changes;
                    if (g_Changes <= kMaxLoggedChanges) {
                        Log::Warn("Physics FPU control word changed 0x%X -> 0x%X before physics step.",
                                  g_LastSeen, current);
                    }
                    g_LastSeen = current;
                }
            }
            return s_Hook.InvokeMethodOriginal(this);
        }

        static Hook<MethodType> s_Hook;
    };

    Hook<IpionPostProcessHook::MethodType> IpionPostProcessHook::s_Hook;
}

namespace PhysicsDeterminism {
    bool EnableFpuMonitor(CKBaseManager *ipionManager) {
        if (!ipionManager) return false;
        if (IpionPostProcessHook::s_Hook.GetOriginal()) return true;

        // CKBaseManager vtable: PreProcess is slot 5, PostProcess slot 6
        // (verified against ??_7CKIpionManager@@6B@ in physics_RT.dll).
        void **vtable = *reinterpret_cast<void ***>(ipionManager);
        auto postProcess = *reinterpret_cast<IpionPostProcessHook::MethodType *>(&vtable[6]);
        return IpionPostProcessHook::s_Hook.Enable(postProcess, &IpionPostProcessHook::Detour);
    }

    void DisableFpuMonitor() {
        IpionPostProcessHook::s_Hook.Disable();
        g_Monitoring = false;
    }

    void SetFpuMonitoring(bool enabled) {
        if (g_Monitoring && !enabled) {
            if (g_Changes > 0) {
                Log::Info("Physics FPU control word changed %u time(s) during the TAS.", g_Changes);
            }
            g_HasLastSeen = false;
            g_Changes = 0;
        }
        g_Monitoring = enabled;
    }
}

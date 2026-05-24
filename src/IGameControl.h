#pragma once

#include <cstddef>
#include <functional>

enum class UIMode;

class IGameControl {
public:
    virtual ~IGameControl() = default;

    virtual void AcquireGameplayInfo() = 0;
    virtual void AcquireKeyBindings() = 0;
    virtual void ResetPhysicsTime() = 0;
    virtual void SetUIMode(UIMode mode) = 0;
    virtual void AddTimer(size_t tick, const std::function<void()> &callback) = 0;
};

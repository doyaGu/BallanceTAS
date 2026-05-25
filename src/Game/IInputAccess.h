#pragma once

#include <BML/InputHook.h>

class InputHook;

class IInputAccess {
public:
    virtual ~IInputAccess() = default;

    virtual InputHook *GetInputManager() const = 0;
};

#pragma once

#include <cstddef>

/**
 * @file IRNGProvider.h
 * @brief Interface for Random Number Generator state management.
 *
 * Used by systems that need deterministic RNG control for TAS replays.
 */

struct RNGState;

class IRNGProvider {
public:
    virtual ~IRNGProvider() = default;

    virtual RNGState GetRNGState() = 0;
    virtual void PushRNGState() = 0;
    virtual void PopRNGState() = 0;
    virtual void ClearRNGStateStack() = 0;
    virtual size_t GetRNGStateStackDepth() const = 0;
    virtual bool IsRNGStateStackEmpty() const = 0;
    virtual void ResetRNGStateID() = 0;
};

#pragma once

/**
 * @file ITimeProvider.h
 * @brief Interface for Virtools context and time manager access.
 *
 * Provides the minimal dependency for components that need to interact
 * with the CK engine context or time management.
 */

class CKContext;
class CKTimeManager;
class CKRenderContext;

class ITimeProvider {
public:
    virtual ~ITimeProvider() = default;

    virtual CKContext *GetCKContext() const = 0;
    virtual CKRenderContext *GetRenderContext() const = 0;
    virtual CKTimeManager *GetTimeManager() const = 0;
};

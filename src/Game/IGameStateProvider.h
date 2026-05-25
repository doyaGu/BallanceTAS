#pragma once

#include <string>

/**
 * @file IGameStateProvider.h
 * @brief Interface for gameplay state queries (level, score, lives, etc.).
 *
 * Consumers that need to read or write gameplay state depend on this narrow
 * interface instead of the full GameInterface.
 */

class IGameStateProvider {
public:
    virtual ~IGameStateProvider() = default;

    // --- State Queries ---
    virtual bool IsIngame() const = 0;
    virtual bool IsPaused() const = 0;
    virtual bool IsPlaying() const = 0;

    // --- Level/Sector ---
    virtual int GetCurrentLevel() const = 0;
    virtual int GetCurrentSector() const = 0;
    virtual bool SetCurrentSector(int sector) = 0;

    // --- Score & Lives ---
    virtual int GetPoints() const = 0;
    virtual bool SetPoints(int points) = 0;
    virtual int GetLifeCount() const = 0;
    virtual bool SetLifeCount(int lives) = 0;

    virtual float GetSRScore() const = 0;
    virtual bool SetSRScore(float score) = 0;
    virtual int GetHSScore() const = 0;
    virtual bool SetHSScore(int score) = 0;
};

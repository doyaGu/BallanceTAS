#pragma once

#include <string>

class IGameQuery {
public:
    virtual ~IGameQuery() = default;

    virtual const std::string &GetMapName() const = 0;
    virtual int GetCurrentLevel() const = 0;
    virtual int GetCurrentSector() const = 0;
    virtual int GetPoints() const = 0;
    virtual int GetLifeCount() const = 0;
    virtual float GetSRScore() const = 0;
    virtual int GetHSScore() const = 0;
    virtual bool IsIngame() const = 0;
    virtual bool IsPaused() const = 0;
    virtual bool IsPlaying() const = 0;
};

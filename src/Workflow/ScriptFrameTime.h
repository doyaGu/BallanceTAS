#pragma once

/**
 * @class ScriptFrameTime
 * @brief The frame time (ms) a playing script runs at; tas.set_frame_time changes it.
 *
 * Script playback and script-to-record translation give the game this frame time each
 * frame, the way record playback gives it each record frame's own time. A change made
 * during script tick t applies from tick t + 1 on, as record frame t + 1's time would.
 */
class ScriptFrameTime {
public:
    /// A script run starts at its project's frame time.
    void Begin(float frameTimeMs) {
        m_FrameTimeMs = frameTimeMs;
        m_Active = true;
    }

    void End() { m_Active = false; }

    bool IsActive() const { return m_Active; }

    /// Returns false when no script run is in progress.
    bool Set(float frameTimeMs) {
        if (!m_Active) {
            return false;
        }
        m_FrameTimeMs = frameTimeMs;
        return true;
    }

    float Get() const { return m_FrameTimeMs; }

private:
    float m_FrameTimeMs = 0.0f;
    bool m_Active = false;
};

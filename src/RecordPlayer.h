#pragma once

#include <vector>
#include <string>

#include <CKDefines.h>

// Forward declarations
class TASEngine;
class TASProject;

/**
 * @struct RecordKeyState
 * @brief Matches the exact binary format from legacy TAS records.
 */
#pragma pack(push, 1)
struct RecordKeyState {
    unsigned key_up : 1;
    unsigned key_down : 1;
    unsigned key_left : 1;
    unsigned key_right : 1;
    unsigned key_shift : 1;
    unsigned key_space : 1;
    unsigned key_q : 1;
    unsigned key_esc : 1;
    unsigned key_enter : 1;
};
#pragma pack(pop)

/**
 * @struct RecordFrameData
 * @brief Matches the exact binary format from legacy TAS records.
 */
#pragma pack(push, 1)
struct RecordFrameData {
    float deltaTime;
    union {
        RecordKeyState keyState;
        int keyStates;
    };

    RecordFrameData() : deltaTime(0.0f), keyStates(0) {}
    explicit RecordFrameData(float deltaTime) : deltaTime(deltaTime) {
        keyStates = 0;
    }
};
#pragma pack(pop)

/**
 * @class RecordPlayer
 * @brief Handles playback of binary .tas record files.
 *
 * This class loads and plays back legacy TAS records in binary format.
 * It provides frame-by-frame input replay by directly manipulating the
 * keyboard state buffer.
 * Record playback can ONLY be used in legacy mode.
 */
class RecordPlayer {
public:
    explicit RecordPlayer(TASEngine *engine);
    ~RecordPlayer() = default;

    // RecordPlayer is not copyable or movable
    RecordPlayer(const RecordPlayer &) = delete;
    RecordPlayer &operator=(const RecordPlayer &) = delete;

    /**
     * @brief Loads and starts playback of a TAS record project.
     * @param project The record-based TAS project to play.
     * @return True if the record was loaded and playback started successfully.
     */
    bool LoadAndPlay(const TASProject *project);

    /**
     * @brief Stops playback and cleans up.
     */
    void Stop();

    /**
     * @brief Processes one frame of record playback.
     * This applies the input for the current frame and advances to the next.
     * Should be called from the InputManager hook.
     */
    void Tick(size_t currentTick, unsigned char *keyboardState);

    /**
     * @brief Checks if a record is currently playing.
     * @return True if playback is active.
     */
    bool IsPlaying() const { return m_IsPlaying; }

    /**
     * @brief Gets the total number of frames in the loaded record.
     * @return Total frame count, or 0 if no record is loaded.
     */
    size_t GetTotalFrames() const { return m_TotalFrames; }

    /**
     * @brief Gets the delta time for the current frame.
     * Used by TimeManager hook to set the correct frame timing.
     * @return Delta time in milliseconds for the current frame.
     */
    float GetFrameDeltaTime(size_t currentTick) const;

private:
    /**
     * @brief Loads a .tas record file using the legacy format.
     * @param recordPath Path to the .tas file.
     * @return True if the file was loaded successfully.
     */
    bool LoadRecord(const std::string &recordPath);

    /**
     * @brief Applies legacy keyboard state input for the current frame.
     * @param currentFrame The current frame's input data.
     * @param nextFrame The next frame's input data (for state transitions).
     * @param keyboardState The game's keyboard state buffer.
     */
    void ApplyFrameInput(const RecordFrameData &currentFrame, const RecordFrameData &nextFrame, unsigned char *keyboardState);

    /**
     * @brief Converts the current and next key states to a keyboard state byte.
     * @param current The current key state (pressed or not).
     * @param next The next key state (pressed or not).
     * @return KS_PRESSED if the key is currently pressed,
     *         KS_RELEASED if it was just released,
     *         KS_IDLE if it is not pressed.
     */
    static int ConvertKeyState(bool current, bool next) ;

    // Core references
    TASEngine *m_Engine;

    // Record data
    size_t m_TotalFrames = 0;
    std::vector<RecordFrameData> m_Frames;
    bool m_IsPlaying = false;

    // Cached remapped keys (acquired once when playback starts)
    CKKEYBOARD m_KeyUp = CKKEY_UP;
    CKKEYBOARD m_KeyDown = CKKEY_DOWN;
    CKKEYBOARD m_KeyLeft = CKKEY_LEFT;
    CKKEYBOARD m_KeyRight = CKKEY_RIGHT;
    CKKEYBOARD m_KeyShift = CKKEY_LSHIFT;
    CKKEYBOARD m_KeySpace = CKKEY_SPACE;
};
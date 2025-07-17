#include "RecordPlayer.h"

#include <fstream>
#include <stdexcept>

#include "TASEngine.h"
#include "TASProject.h"
#include "GameInterface.h"

RecordPlayer::RecordPlayer(TASEngine *engine) : m_Engine(engine) {
    if (!m_Engine) {
        throw std::runtime_error("RecordPlayer requires a valid TASEngine instance.");
    }
}

bool RecordPlayer::LoadAndPlay(const TASProject *project) {
    if (!project || !project->IsRecordProject() || !project->IsValid()) {
        m_Engine->GetLogger()->Error("Invalid record project provided.");
        return false;
    }

    // Verify legacy mode is enabled (required for record playback)
    if (!m_Engine->GetGameInterface()->IsLegacyMode()) {
        m_Engine->GetLogger()->Error("Record playback requires legacy mode to be enabled.");
        return false;
    }

    // Stop any current playback
    Stop();

    std::string recordPath = project->GetRecordFilePath();
    if (!LoadRecord(recordPath)) {
        m_Engine->GetLogger()->Error("Failed to load record: %s", recordPath.c_str());
        return false;
    }

    // Acquire remapped keys from game interface
    auto *gameInterface = m_Engine->GetGameInterface();
    if (gameInterface) {
        m_KeyUp = gameInterface->RemapKey(CKKEY_UP);
        m_KeyDown = gameInterface->RemapKey(CKKEY_DOWN);
        m_KeyLeft = gameInterface->RemapKey(CKKEY_LEFT);
        m_KeyRight = gameInterface->RemapKey(CKKEY_RIGHT);
        m_KeyShift = gameInterface->RemapKey(CKKEY_LSHIFT);
        m_KeySpace = gameInterface->RemapKey(CKKEY_SPACE);
    }

    m_IsPlaying = true;

    m_Engine->GetLogger()->Info("Record '%s' loaded and playback started (%zu frames).",
                                project->GetName().c_str(), m_TotalFrames);
    return true;
}

void RecordPlayer::Stop() {
    if (!m_IsPlaying) return;

    m_IsPlaying = false;
    m_TotalFrames = 0;
    m_Frames.clear();
    m_Frames.shrink_to_fit();

    m_Engine->GetLogger()->Info("Record playback stopped.");
}

void RecordPlayer::Tick(size_t currentTick, unsigned char *keyboardState) {
    if (!m_IsPlaying) {
        return;
    }

    // Check if we've reached the end
    if (currentTick >= m_TotalFrames) {
        m_Engine->GetLogger()->Info("Record playback completed naturally.");
        Stop();
        return;
    }

    // Apply input for the current frame and advance
    ApplyFrameInput(m_Frames[currentTick], m_Frames[currentTick + 1], keyboardState);
}

float RecordPlayer::GetFrameDeltaTime(size_t currentTick) const {
    if (!m_IsPlaying || currentTick >= m_TotalFrames) {
        return 1000.0f / 132.0f; // Default delta time
    }
    return m_Frames[currentTick].deltaTime;
}

bool RecordPlayer::LoadRecord(const std::string &recordPath) {
    try {
        m_Engine->GetLogger()->Info("Loading TAS record: %s", recordPath.c_str());

        // --- 1. Read the entire file ---
        std::ifstream file(recordPath, std::ios::binary);
        if (!file) {
            m_Engine->GetLogger()->Error("Could not open record file: %s", recordPath.c_str());
            return false;
        }

        // --- 2. Read the 4-byte header for uncompressed size (legacy format) ---
        uint32_t uncompressedSize;
        file.read(reinterpret_cast<char *>(&uncompressedSize), sizeof(uncompressedSize));
        if (file.gcount() != sizeof(uncompressedSize)) {
            m_Engine->GetLogger()->Error("Failed to read uncompressed size header from file.");
            return false;
        }

        if (uncompressedSize == 0) {
            m_Engine->GetLogger()->Warn("Record file is empty.");
            m_TotalFrames = 0;
            m_Frames.clear();
            return true; // Empty recording is technically valid
        }

        // The uncompressed data must be a multiple of the FrameData size.
        if (uncompressedSize % sizeof(RecordFrameData) != 0) {
            m_Engine->GetLogger()->Error(
                "Uncompressed size is not a multiple of FrameData size. File may be corrupt.");
            return false;
        }

        // --- 3. Read the compressed payload ---
        file.seekg(0, std::ios::end);
        std::streampos fileSize = file.tellg();
        file.seekg(sizeof(uncompressedSize), std::ios::beg);

        size_t compressedSize = static_cast<size_t>(fileSize) - sizeof(uncompressedSize);
        std::vector<char> compressedData(compressedSize);
        file.read(compressedData.data(), compressedSize);
        if (static_cast<size_t>(file.gcount()) != compressedSize) {
            m_Engine->GetLogger()->Error("Failed to read compressed data payload.");
            return false;
        }
        file.close();

        // --- 4. Decompress using CKUnPackData function ---
        char *uncompressedData = CKUnPackData(static_cast<int>(uncompressedSize), compressedData.data(), compressedSize);
        if (!uncompressedData) {
            m_Engine->GetLogger()->Error("Failed to decompress TAS record data using CKUnPackData.");
            return false;
        }

        // --- 5. Copy the decompressed data to our frame vector ---
        size_t frameCount = uncompressedSize / sizeof(RecordFrameData);
        m_TotalFrames = frameCount;
        m_Frames.resize(frameCount + 1); // +1 for the next frame input
        memcpy(m_Frames.data(), uncompressedData, uncompressedSize);

        // Clean up the decompressed data
        CKDeletePointer(uncompressedData);

        m_Engine->GetLogger()->Info("Record loaded successfully: %zu frames", m_TotalFrames);
        return true;
    } catch (const std::exception &e) {
        m_Engine->GetLogger()->Error("Exception loading record: %s", e.what());
        return false;
    }
}

void RecordPlayer::ApplyFrameInput(const RecordFrameData &currentFrame,
                                   const RecordFrameData &nextFrame,
                                   unsigned char *keyboardState) const {
    if (!keyboardState) {
        m_Engine->GetLogger()->Error("Keyboard state buffer is null. Cannot apply input.");
        return;
    }

    // We directly set the keyboard state bytes based on the key state bits
    const RecordKeyState &current = currentFrame.keyState;
    const RecordKeyState &next = nextFrame.keyState;

    // Set keyboard state
    keyboardState[m_KeyUp] = ConvertKeyState(current.key_up, next.key_up);
    keyboardState[m_KeyDown] = ConvertKeyState(current.key_down, next.key_down);
    keyboardState[m_KeyLeft] = ConvertKeyState(current.key_left, next.key_left);
    keyboardState[m_KeyRight] = ConvertKeyState(current.key_right, next.key_right);
    keyboardState[CKKEY_Q] = ConvertKeyState(current.key_q, next.key_q);
    keyboardState[m_KeyShift] = ConvertKeyState(current.key_shift, next.key_shift);
    keyboardState[m_KeySpace] = ConvertKeyState(current.key_space, next.key_space);
    keyboardState[CKKEY_ESCAPE] = ConvertKeyState(current.key_esc, next.key_esc);
}

int RecordPlayer::ConvertKeyState(bool current, bool next) {
    int state = KS_IDLE; // Default to idle state
    if (current != KS_IDLE) {
        state |= KS_PRESSED; // Key is currently pressed
        if (next == KS_IDLE) {
            state |= KS_RELEASED; // Key was just released
        }
    }
    return state;
}

#pragma once

#include <filesystem>
#include <vector>

#include <CKDefines.h>

#include "RecordPlayer.h"
#include "Recorder.h"
#include "Result.h"

namespace tas::record {

struct RecordInputMapping {
    CKKEYBOARD keyUp = CKKEY_UP;
    CKKEYBOARD keyDown = CKKEY_DOWN;
    CKKEYBOARD keyLeft = CKKEY_LEFT;
    CKKEYBOARD keyRight = CKKEY_RIGHT;
    CKKEYBOARD keyShift = CKKEY_LSHIFT;
    CKKEYBOARD keySpace = CKKEY_SPACE;
    CKKEYBOARD keyQ = CKKEY_Q;
    CKKEYBOARD keyEsc = CKKEY_ESCAPE;
};

RecordFrameData CaptureKeyboardStateToRecordFrame(const unsigned char *keyboardState,
                                                  const RecordInputMapping &mapping,
                                                  float deltaTimeMs);

RecordFrameData ConvertFrameDataToRecordFrame(const FrameData &frame, float deltaTimeMs);

std::vector<RecordFrameData> ConvertFrameDataToRecordFrames(const std::vector<FrameData> &frames,
                                                            float deltaTimeMs);

Result<void> WriteLegacyRecordFile(const std::filesystem::path &path,
                                   const std::vector<RecordFrameData> &frames);

} // namespace tas::record

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

Result<std::vector<RecordFrameData>> ReadLegacyRecordFile(const std::filesystem::path &path);

/// Frame-by-frame difference of two records: keys and frame time, exactly.
struct RecordComparison {
    bool identical = false;      ///< Every compared frame matches and (unless prefix) lengths match.
    size_t framesA = 0;
    size_t framesB = 0;
    size_t comparedFrames = 0;   ///< min(framesA, framesB)
    size_t differingFrames = 0;
    bool hasDifference = false;
    size_t firstDifference = 0;  ///< First differing frame; framesA/framesB-wide when only lengths differ.
    RecordFrameData frameA;      ///< The frames at firstDifference (zero past a record's end).
    RecordFrameData frameB;
};

/// The key bits a record frame stores.
constexpr int kRecordKeyMask = 0x1FF;

/// With prefixOnly, frames past the shorter record are ignored (a translation ends at the
/// level finish, while the source record may run on).
RecordComparison CompareRecordFrames(const std::vector<RecordFrameData> &a,
                                     const std::vector<RecordFrameData> &b,
                                     bool prefixOnly);

} // namespace tas::record

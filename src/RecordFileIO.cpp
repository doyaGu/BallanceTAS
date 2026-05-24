#include "RecordFileIO.h"

#include <cstdint>
#include <fstream>
#include <limits>

#include <CKGlobals.h>

namespace tas::record {
namespace {

bool IsPressed(unsigned char state) {
    return (state & KS_PRESSED) != 0;
}

bool ReadPressed(const unsigned char *keyboardState, CKKEYBOARD key) {
    if (!keyboardState) {
        return false;
    }
    const auto index = static_cast<unsigned int>(key);
    if (index >= 256u) {
        return false;
    }
    return IsPressed(keyboardState[index]);
}

bool RawPressed(uint8_t state) {
    return (state & KS_PRESSED) != 0;
}

bool CanUseLegacyPackSize(size_t byteCount) {
    return byteCount <= static_cast<size_t>((std::numeric_limits<int>::max)())
        && byteCount <= static_cast<size_t>((std::numeric_limits<uint32_t>::max)());
}

} // namespace

RecordFrameData CaptureKeyboardStateToRecordFrame(const unsigned char *keyboardState,
                                                  const RecordInputMapping &mapping,
                                                  float deltaTimeMs) {
    RecordFrameData frame(deltaTimeMs);
    frame.keyState.key_up = ReadPressed(keyboardState, mapping.keyUp);
    frame.keyState.key_down = ReadPressed(keyboardState, mapping.keyDown);
    frame.keyState.key_left = ReadPressed(keyboardState, mapping.keyLeft);
    frame.keyState.key_right = ReadPressed(keyboardState, mapping.keyRight);
    frame.keyState.key_shift = ReadPressed(keyboardState, mapping.keyShift);
    frame.keyState.key_space = ReadPressed(keyboardState, mapping.keySpace);
    frame.keyState.key_q = ReadPressed(keyboardState, mapping.keyQ);
    frame.keyState.key_esc = ReadPressed(keyboardState, mapping.keyEsc);
    return frame;
}

RecordFrameData ConvertFrameDataToRecordFrame(const FrameData &frame, float deltaTimeMs) {
    RecordFrameData recordFrame(deltaTimeMs);
    recordFrame.keyState.key_up = RawPressed(frame.inputState.keyUp);
    recordFrame.keyState.key_down = RawPressed(frame.inputState.keyDown);
    recordFrame.keyState.key_left = RawPressed(frame.inputState.keyLeft);
    recordFrame.keyState.key_right = RawPressed(frame.inputState.keyRight);
    recordFrame.keyState.key_shift = RawPressed(frame.inputState.keyShift);
    recordFrame.keyState.key_space = RawPressed(frame.inputState.keySpace);
    recordFrame.keyState.key_q = RawPressed(frame.inputState.keyQ);
    recordFrame.keyState.key_esc = RawPressed(frame.inputState.keyEsc);
    return recordFrame;
}

std::vector<RecordFrameData> ConvertFrameDataToRecordFrames(const std::vector<FrameData> &frames,
                                                            float deltaTimeMs) {
    std::vector<RecordFrameData> converted;
    converted.reserve(frames.size());
    for (const auto &frame : frames) {
        converted.push_back(ConvertFrameDataToRecordFrame(frame, deltaTimeMs));
    }
    return converted;
}

Result<void> WriteLegacyRecordFile(const std::filesystem::path &path,
                                   const std::vector<RecordFrameData> &frames) {
    if (frames.empty()) {
        return Result<void>::Error("Record has no frames", "record_file");
    }

    const size_t uncompressedSize = frames.size() * sizeof(RecordFrameData);
    if (!CanUseLegacyPackSize(uncompressedSize)) {
        return Result<void>::Error("Record is too large for legacy format", "record_file");
    }

    std::filesystem::create_directories(path.parent_path());

    int compressedSize = 0;
    char *compressedData = CKPackData(
        const_cast<char *>(reinterpret_cast<const char *>(frames.data())),
        static_cast<int>(uncompressedSize),
        compressedSize,
        9);

    if (!compressedData || compressedSize <= 0) {
        return Result<void>::Error("Failed to compress record data", "record_file");
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        CKDeletePointer(compressedData);
        return Result<void>::Error("Failed to open record file for writing", "record_file");
    }

    const auto header = static_cast<uint32_t>(uncompressedSize);
    file.write(reinterpret_cast<const char *>(&header), sizeof(header));
    file.write(compressedData, compressedSize);
    const bool ok = file.good();
    file.close();

    CKDeletePointer(compressedData);

    if (!ok) {
        std::filesystem::remove(path);
        return Result<void>::Error("Failed to write record file", "record_file");
    }

    return Result<void>::Ok();
}

} // namespace tas::record

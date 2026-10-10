#include "RecordFileIO.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>

#include <CKGlobals.h>

namespace tas::record {

static bool IsPressed(unsigned char state) {
    return (state & KS_PRESSED) != 0;
}

static bool ReadPressed(const unsigned char *keyboardState, CKKEYBOARD key) {
    if (!keyboardState) {
        return false;
    }
    const auto index = static_cast<unsigned int>(key);
    if (index >= 256u) {
        return false;
    }
    return IsPressed(keyboardState[index]);
}

static bool RawPressed(uint8_t state) {
    return (state & KS_PRESSED) != 0;
}

static bool CanUseLegacyPackSize(size_t byteCount) {
    return byteCount <= static_cast<size_t>((std::numeric_limits<int>::max)())
        && byteCount <= static_cast<size_t>((std::numeric_limits<uint32_t>::max)());
}

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

Result<std::vector<RecordFrameData>> ReadLegacyRecordFile(const std::filesystem::path &path) {
    using FramesResult = Result<std::vector<RecordFrameData>>;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return FramesResult::Error("Could not open record file: " + path.string(), "record_file");
    }
    const std::streamoff fileSize = file.tellg();
    uint32_t uncompressedSize = 0;
    if (fileSize <= static_cast<std::streamoff>(sizeof(uncompressedSize))) {
        return FramesResult::Error("Record file is truncated: " + path.string(), "record_file");
    }
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(&uncompressedSize), sizeof(uncompressedSize));
    if (uncompressedSize == 0 || uncompressedSize % sizeof(RecordFrameData) != 0 ||
        !CanUseLegacyPackSize(uncompressedSize)) {
        return FramesResult::Error("Record header is invalid: " + path.string(), "record_file");
    }

    std::vector<char> compressed(static_cast<size_t>(fileSize) - sizeof(uncompressedSize));
    file.read(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    if (static_cast<size_t>(file.gcount()) != compressed.size()) {
        return FramesResult::Error("Record payload is incomplete: " + path.string(), "record_file");
    }

    char *data = CKUnPackData(static_cast<int>(uncompressedSize), compressed.data(),
                              static_cast<int>(compressed.size()));
    if (!data) {
        return FramesResult::Error("Record payload could not be decompressed: " + path.string(), "record_file");
    }
    std::vector<RecordFrameData> frames(uncompressedSize / sizeof(RecordFrameData));
    std::memcpy(frames.data(), data, uncompressedSize);
    CKDeletePointer(data);
    return FramesResult::Ok(std::move(frames));
}

static bool SameFrame(const RecordFrameData &a, const RecordFrameData &b) {
    return (a.keyStates & kRecordKeyMask) == (b.keyStates & kRecordKeyMask) && a.deltaTime == b.deltaTime;
}

RecordComparison CompareRecordFrames(const std::vector<RecordFrameData> &a,
                                     const std::vector<RecordFrameData> &b,
                                     bool prefixOnly) {
    RecordComparison result;
    result.framesA = a.size();
    result.framesB = b.size();
    result.comparedFrames = (std::min)(a.size(), b.size());
    for (size_t frame = 0; frame < result.comparedFrames; ++frame) {
        if (SameFrame(a[frame], b[frame])) {
            continue;
        }
        if (!result.hasDifference) {
            result.hasDifference = true;
            result.firstDifference = frame;
            result.frameA = a[frame];
            result.frameB = b[frame];
        }
        ++result.differingFrames;
    }
    if (!result.hasDifference && !prefixOnly && a.size() != b.size()) {
        result.hasDifference = true;
        result.firstDifference = result.comparedFrames;
        if (result.comparedFrames < a.size()) {
            result.frameA = a[result.comparedFrames];
        }
        if (result.comparedFrames < b.size()) {
            result.frameB = b[result.comparedFrames];
        }
    }
    result.identical = !result.hasDifference;
    return result;
}

} // namespace tas::record

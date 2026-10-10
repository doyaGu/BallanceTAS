#include "RecordFileIO.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include <CKGlobals.h>
#include <gtest/gtest.h>

#include "TASProject.h"

static std::filesystem::path TempRecordPath(const std::string &name) {
    auto path = std::filesystem::temp_directory_path() / "BallanceTAS_RecordFileIO";
    std::filesystem::create_directories(path);
    return path / name;
}

static std::vector<RecordFrameData> ReadLegacyRecord(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    uint32_t uncompressedSize = 0;
    file.read(reinterpret_cast<char *>(&uncompressedSize), sizeof(uncompressedSize));

    file.seekg(0, std::ios::end);
    const auto fileSize = file.tellg();
    file.seekg(sizeof(uncompressedSize), std::ios::beg);

    const auto compressedSize = static_cast<int>(fileSize - static_cast<std::streamoff>(sizeof(uncompressedSize)));
    std::vector<char> compressed(static_cast<size_t>(compressedSize));
    file.read(compressed.data(), compressedSize);

    char *uncompressed = CKUnPackData(static_cast<int>(uncompressedSize), compressed.data(), compressedSize);
    EXPECT_NE(uncompressed, nullptr);

    std::vector<RecordFrameData> frames(uncompressedSize / sizeof(RecordFrameData));
    std::memcpy(frames.data(), uncompressed, uncompressedSize);
    CKDeletePointer(uncompressed);
    return frames;
}

TEST(RecordFileIOTest, RejectsEmptyRecordWrites) {
    const auto path = TempRecordPath("empty_output.tas");
    std::filesystem::remove(path);

    const auto result = tas::record::WriteLegacyRecordFile(path, {});

    EXPECT_FALSE(result.IsOk());
    EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(RecordFileIOTest, WritesLegacyRecordReadableByTASProject) {
    const auto path = TempRecordPath("captured_output.tas");
    std::filesystem::remove(path);

    RecordFrameData first(1000.0f / 132.0f);
    first.keyState.key_up = 1;
    first.keyState.key_space = 1;
    RecordFrameData second(1000.0f / 132.0f);
    second.keyState.key_right = 1;
    second.keyState.key_q = 1;

    std::vector<RecordFrameData> frames(16, RecordFrameData(1000.0f / 132.0f));
    frames[0] = first;
    frames[1] = second;

    const auto result = tas::record::WriteLegacyRecordFile(path, frames);

    ASSERT_TRUE(result.IsOk()) << result.GetError().message;
    TASProject project(path.string());
    EXPECT_TRUE(project.IsValid()) << project.GetValidationMessage();
    EXPECT_EQ(project.GetRecordFrameCount(), 16u);

    const auto loadedFrames = ReadLegacyRecord(path);
    ASSERT_EQ(loadedFrames.size(), 16u);
    EXPECT_TRUE(loadedFrames[0].keyState.key_up);
    EXPECT_TRUE(loadedFrames[0].keyState.key_space);
    EXPECT_FALSE(loadedFrames[0].keyState.key_right);
    EXPECT_TRUE(loadedFrames[1].keyState.key_right);
    EXPECT_TRUE(loadedFrames[1].keyState.key_q);
}

TEST(RecordFileIOTest, CapturesKeyboardBufferAsRecordBits) {
    unsigned char keyboard[256] = {};
    keyboard[CKKEY_UP] = KS_PRESSED;
    keyboard[CKKEY_LEFT] = KS_PRESSED | KS_RELEASED;
    keyboard[CKKEY_Q] = KS_RELEASED;
    keyboard[CKKEY_ESCAPE] = KS_PRESSED;

    const auto frame = tas::record::CaptureKeyboardStateToRecordFrame(
        keyboard,
        tas::record::RecordInputMapping{},
        7.5f);

    EXPECT_FLOAT_EQ(frame.deltaTime, 7.5f);
    EXPECT_TRUE(frame.keyState.key_up);
    EXPECT_TRUE(frame.keyState.key_left);
    EXPECT_FALSE(frame.keyState.key_q);
    EXPECT_TRUE(frame.keyState.key_esc);
}

static RecordFrameData Frame(float deltaTime, int keys) {
    RecordFrameData frame(deltaTime);
    frame.keyStates = keys;
    return frame;
}

TEST(RecordFileIOTest, ReadsBackWhatItWrites) {
    const auto path = TempRecordPath("roundtrip.tas");
    const std::vector<RecordFrameData> frames = {Frame(4.1666665f, 0), Frame(4.1666665f, 1), Frame(8.0f, 0x30)};
    ASSERT_TRUE(tas::record::WriteLegacyRecordFile(path, frames).IsOk());

    auto read = tas::record::ReadLegacyRecordFile(path);
    ASSERT_TRUE(read.IsOk());
    const auto &back = read.Unwrap();
    ASSERT_EQ(back.size(), frames.size());
    EXPECT_TRUE(tas::record::CompareRecordFrames(frames, back, false).identical);
    std::filesystem::remove(path);

    EXPECT_FALSE(tas::record::ReadLegacyRecordFile(TempRecordPath("missing.tas")).IsOk());
}

TEST(RecordFileIOTest, ComparesKeysAndFrameTimesExactly) {
    const std::vector<RecordFrameData> base = {Frame(4.0f, 0), Frame(4.0f, 1), Frame(4.0f, 1), Frame(4.0f, 0)};

    auto keys = base;
    keys[2].keyStates = 3;
    auto result = tas::record::CompareRecordFrames(base, keys, false);
    EXPECT_FALSE(result.identical);
    EXPECT_EQ(result.firstDifference, 2u);
    EXPECT_EQ(result.differingFrames, 1u);
    EXPECT_EQ(result.frameB.keyStates, 3);

    auto times = base;
    times[1].deltaTime = std::nextafter(4.0f, 5.0f);
    result = tas::record::CompareRecordFrames(base, times, false);
    EXPECT_FALSE(result.identical);
    EXPECT_EQ(result.firstDifference, 1u);

    // Bits past the nine stored keys are not part of a frame.
    auto padding = base;
    padding[0].keyStates |= 1 << 12;
    EXPECT_TRUE(tas::record::CompareRecordFrames(base, padding, false).identical);
}

TEST(RecordFileIOTest, PrefixComparisonIgnoresTheLongerTail) {
    const std::vector<RecordFrameData> longer = {Frame(4.0f, 1), Frame(4.0f, 0), Frame(4.0f, 2)};
    const std::vector<RecordFrameData> shorter = {Frame(4.0f, 1), Frame(4.0f, 0)};

    auto full = tas::record::CompareRecordFrames(longer, shorter, false);
    EXPECT_FALSE(full.identical);
    EXPECT_EQ(full.firstDifference, 2u);
    EXPECT_EQ(full.differingFrames, 0u);

    auto prefix = tas::record::CompareRecordFrames(longer, shorter, true);
    EXPECT_TRUE(prefix.identical);
    EXPECT_EQ(prefix.comparedFrames, 2u);
}

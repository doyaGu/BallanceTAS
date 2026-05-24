#include "RecordFileIO.h"

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

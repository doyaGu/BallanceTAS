#include "InputSystem.h"

#include <gtest/gtest.h>

#include <CKInputManager.h>

#include <cstring>
#include <type_traits>

static_assert(std::is_invocable_v<decltype(&InputSystem::Apply), InputSystem *, size_t, CKInputManager *>);

TEST(InputSystemTest, ApplyWritesKeyboardStateBuffer) {
    InputSystem input;
    unsigned char keyboardState[256];
    std::memset(keyboardState, KS_IDLE, sizeof(keyboardState));

    input.SetEnabled(true);
    input.PressKeys("up space");

    input.ApplyToKeyboardState(1, keyboardState);

    EXPECT_EQ(keyboardState[CKKEY_UP], KS_PRESSED);
    EXPECT_EQ(keyboardState[CKKEY_SPACE], KS_PRESSED);
    EXPECT_EQ(keyboardState[CKKEY_DOWN], KS_IDLE);
}

TEST(InputSystemTest, OneFramePressEmitsReleaseOnNextApplyThenIdle) {
    InputSystem input;
    unsigned char keyboardState[256];
    std::memset(keyboardState, KS_IDLE, sizeof(keyboardState));

    input.SetEnabled(true);
    input.PressKeysOneFrame("left");

    input.ApplyToKeyboardState(1, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_LEFT], (KS_PRESSED | KS_RELEASED));

    input.ApplyToKeyboardState(2, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_LEFT], KS_IDLE);
}

TEST(InputSystemTest, MergedInputsKeepLowerPriorityExplicitKeys) {
    InputSystem global;
    InputSystem level;
    unsigned char keyboardState[256];
    std::memset(keyboardState, KS_IDLE, sizeof(keyboardState));

    global.SetEnabled(true);
    level.SetEnabled(true);
    global.PressKeys("up");
    level.PressKeys("space");

    InputSystem::ApplyMergedToKeyboardState(1, {&level, &global}, keyboardState);

    EXPECT_EQ(keyboardState[CKKEY_UP], KS_PRESSED);
    EXPECT_EQ(keyboardState[CKKEY_SPACE], KS_PRESSED);
    EXPECT_EQ(keyboardState[CKKEY_DOWN], KS_IDLE);
}

TEST(InputSystemTest, HigherPriorityExplicitKeyOverridesLowerPriorityKey) {
    InputSystem global;
    InputSystem level;
    unsigned char keyboardState[256];
    std::memset(keyboardState, KS_IDLE, sizeof(keyboardState));

    global.SetEnabled(true);
    level.SetEnabled(true);
    global.PressKeys("up");
    level.ReleaseKeys("up");

    InputSystem::ApplyMergedToKeyboardState(1, {&level, &global}, keyboardState);

    EXPECT_EQ(keyboardState[CKKEY_UP], KS_RELEASED);
}

TEST(InputSystemTest, ReleaseAfterHeldKeyWritesReleaseOnThatApply) {
    InputSystem input;
    unsigned char keyboardState[256];
    std::memset(keyboardState, KS_IDLE, sizeof(keyboardState));

    input.SetEnabled(true);
    input.PressKeys("up");
    input.ApplyToKeyboardState(1, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_UP], KS_PRESSED);

    input.ReleaseKeys("up");
    input.ApplyToKeyboardState(2, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_UP], (KS_PRESSED | KS_RELEASED));

    input.ApplyToKeyboardState(3, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_UP], KS_IDLE);
}

TEST(InputSystemTest, OneFramePressFromHighPriorityDoesNotEraseLowerPriorityExplicitKey) {
    InputSystem global;
    InputSystem level;
    unsigned char keyboardState[256];
    std::memset(keyboardState, KS_IDLE, sizeof(keyboardState));

    global.SetEnabled(true);
    level.SetEnabled(true);
    global.PressKeys("up");
    level.PressKeysOneFrame("space");

    InputSystem::ApplyMergedToKeyboardState(1, {&level, &global}, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_SPACE], (KS_PRESSED | KS_RELEASED));
    EXPECT_EQ(keyboardState[CKKEY_UP], KS_PRESSED);

    InputSystem::ApplyMergedToKeyboardState(2, {&level, &global}, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_SPACE], KS_IDLE);
    EXPECT_EQ(keyboardState[CKKEY_UP], KS_PRESSED);

    InputSystem::ApplyMergedToKeyboardState(3, {&level, &global}, keyboardState);
    EXPECT_EQ(keyboardState[CKKEY_SPACE], KS_IDLE);
    EXPECT_EQ(keyboardState[CKKEY_UP], KS_PRESSED);
}

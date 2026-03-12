#pragma once

#include <string>

/**
 * @file GameEvents.h
 * @brief Typed C++ event structs for the EventBus, replacing string-keyed game events.
 *
 * Each struct represents a specific game lifecycle event. The struct name is the
 * canonical event type; the `name` field preserves the legacy string for Lua-side
 * EventManager forwarding.
 *
 * Convention:
 *   - Struct name: PascalCase, suffixed with "Event"
 *   - `name` field: matches the old string key (e.g., "start_level")
 *   - Data fields: carry relevant context so subscribers don't need to query GameInterface
 */

// ============================================================================
// Menu Events
// ============================================================================

struct PreStartMenuEvent {
    static constexpr const char *name = "pre_start_menu";
};

struct PostStartMenuEvent {
    static constexpr const char *name = "post_start_menu";
};

// ============================================================================
// Level Lifecycle Events
// ============================================================================

struct PreLoadLevelEvent {
    static constexpr const char *name = "pre_load_level";
};

struct PostLoadLevelEvent {
    static constexpr const char *name = "post_load_level";
};

struct StartLevelEvent {
    static constexpr const char *name = "start_level";
};

struct PreResetLevelEvent {
    static constexpr const char *name = "pre_reset_level";
};

struct PostResetLevelEvent {
    static constexpr const char *name = "post_reset_level";
};

struct PauseLevelEvent {
    static constexpr const char *name = "pause_level";
};

struct UnpauseLevelEvent {
    static constexpr const char *name = "unpause_level";
};

struct PreExitLevelEvent {
    static constexpr const char *name = "pre_exit_level";
};

struct PostExitLevelEvent {
    static constexpr const char *name = "post_exit_level";
};

struct PreNextLevelEvent {
    static constexpr const char *name = "pre_next_level";
};

struct PostNextLevelEvent {
    static constexpr const char *name = "post_next_level";
};

// ============================================================================
// Level Completion Events
// ============================================================================

struct PreEndLevelEvent {
    static constexpr const char *name = "pre_level_end";
};

struct PostEndLevelEvent {
    static constexpr const char *name = "post_level_end";
};

struct LevelFinishEvent {
    static constexpr const char *name = "level_finish";
};

// ============================================================================
// Gameplay State Events
// ============================================================================

struct BallOffEvent {
    static constexpr const char *name = "ball_off";
};

struct GameOverEvent {
    static constexpr const char *name = "game_over";
};

struct ExitGameEvent {
    // No legacy name — this event has no Lua-side equivalent (just clears m_Level01)
};

// ============================================================================
// Checkpoint Events
// ============================================================================

struct PreCheckpointReachedEvent {
    static constexpr const char *name = "pre_checkpoint_reached";
    int sector = -1;
};

struct PostCheckpointReachedEvent {
    static constexpr const char *name = "post_checkpoint_reached";
    int sector = -1;
};

// ============================================================================
// Score & Life Events
// ============================================================================

struct ExtraPointEvent {
    static constexpr const char *name = "extra_point";
    int points = 0;
};

struct PreSubLifeEvent {
    static constexpr const char *name = "pre_sub_life";
    int lifeCount = 0;
};

struct PostSubLifeEvent {
    static constexpr const char *name = "post_sub_life";
    int lifeCount = 0;
};

struct PreLifeUpEvent {
    static constexpr const char *name = "pre_life_up";
    int lifeCount = 0;
};

struct PostLifeUpEvent {
    static constexpr const char *name = "post_life_up";
    int lifeCount = 0;
};

// ============================================================================
// Navigation & Counter Events
// ============================================================================

struct CounterActiveEvent {
    static constexpr const char *name = "counter_active";
};

struct CounterInactiveEvent {
    static constexpr const char *name = "counter_inactive";
};

struct BallNavActiveEvent {
    static constexpr const char *name = "ball_nav_active";
};

struct BallNavInactiveEvent {
    static constexpr const char *name = "ball_nav_inactive";
};

struct CamNavActiveEvent {
    static constexpr const char *name = "cam_nav_active";
};

struct CamNavInactiveEvent {
    static constexpr const char *name = "cam_nav_inactive";
};

// ============================================================================
// Framework Internal Events (not forwarded from BML — used between components)
// ============================================================================

/** Published by ConfigService when any config property changes at runtime. */
struct ConfigChangedEvent {
    const char *category = nullptr;
    const char *key = nullptr;
};

/** Published when a TAS operation state changes (recording/playback/idle). */
struct TASStateChangedEvent {
    int previousState = 0;
    int newState = 0;
};

/** Published when playback finishes naturally and the state machine should stop it. */
struct PlaybackCompletedEvent {
    int playbackType = 0;
};

/** Published when translation finishes naturally and the state machine should stop it. */
struct TranslationCompletedEvent {
};

/** Published when validation recording starts. */
struct ValidationStartedEvent {
    std::string outputPath;
};

/** Published when validation recording stops. */
struct ValidationStoppedEvent {
    std::string outputPath;
    bool success = false;
};

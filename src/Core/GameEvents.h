#pragma once

#include <cstddef>
#include <optional>
#include <string>

/**
 * @file GameEvents.h
 * @brief Typed C++ event structs for the EventBus, replacing string-keyed game events.
 *
 * Simple no-data events are generated via the TAS_SIMPLE_GAME_EVENTS X-macro.
 * Events carrying data fields are defined manually below.
 */

// ============================================================================
// X-macro: simple no-data game events
//   X(StructPrefix, EnumValue, legacy_name_string)
// ============================================================================
#define TAS_SIMPLE_GAME_EVENTS(X) \
    X(PreStartMenu,        PreStartMenu,        "pre_start_menu")        \
    X(PostStartMenu,       PostStartMenu,       "post_start_menu")       \
    X(PreLoadLevel,        PreLoadLevel,        "pre_load_level")        \
    X(PostLoadLevel,       PostLoadLevel,       "post_load_level")       \
    X(StartLevel,          StartLevel,          "start_level")           \
    X(PreResetLevel,       PreResetLevel,       "pre_reset_level")       \
    X(PostResetLevel,      PostResetLevel,      "post_reset_level")      \
    X(PauseLevel,          PauseLevel,          "pause_level")           \
    X(UnpauseLevel,        UnpauseLevel,        "unpause_level")         \
    X(PreExitLevel,        PreExitLevel,        "pre_exit_level")        \
    X(PostExitLevel,       PostExitLevel,       "post_exit_level")       \
    X(PreNextLevel,        PreNextLevel,        "pre_next_level")        \
    X(PostNextLevel,       PostNextLevel,       "post_next_level")       \
    X(PreEndLevel,         PreEndLevel,         "pre_level_end")         \
    X(PostEndLevel,        PostEndLevel,        "post_level_end")        \
    X(LevelFinish,         LevelFinish,         "level_finish")          \
    X(Dead,                Dead,                "dead")                  \
    X(BallOff,             BallOff,             "ball_off")              \
    X(GameOver,            GameOver,            "game_over")             \
    X(CounterActive,       CounterActive,       "counter_active")        \
    X(CounterInactive,     CounterInactive,     "counter_inactive")      \
    X(BallNavActive,       BallNavActive,       "ball_nav_active")       \
    X(BallNavInactive,     BallNavInactive,     "ball_nav_inactive")     \
    X(CamNavActive,        CamNavActive,        "cam_nav_active")        \
    X(CamNavInactive,      CamNavInactive,      "cam_nav_inactive")

// ============================================================================
// GameEventType enum — includes both simple and data-carrying events
// ============================================================================
enum class GameEventType {
#define ENUM_ENTRY(Prefix, Enum, Name) Enum,
    TAS_SIMPLE_GAME_EVENTS(ENUM_ENTRY)
#undef ENUM_ENTRY
    // Data-carrying events
    PreCheckpointReached,
    PostCheckpointReached,
    ExtraPoint,
    PreSubLife,
    PostSubLife,
    PreLifeUp,
    PostLifeUp
};

// ============================================================================
// LuaGameEvent — unified event struct forwarded to Lua scripts
// ============================================================================
struct LuaGameEvent {
    GameEventType type;
    const char *name = "";
    size_t tick = 0;
    std::optional<int> sector;
    std::optional<int> points;
    std::optional<int> lifeCount;
};

// ============================================================================
// EventTypeTraits — maps event struct -> enum + legacy name
// ============================================================================
template <typename EventT>
struct EventTypeTraits;

// ============================================================================
// Generate simple event structs + traits via X-macro
// ============================================================================
#define DEFINE_SIMPLE_EVENT(Prefix, Enum, LegacyName)                   \
    struct Prefix##Event {                                              \
        static constexpr const char *name = LegacyName;                \
    };                                                                  \
    template <>                                                         \
    struct EventTypeTraits<Prefix##Event> {                             \
        static constexpr GameEventType type = GameEventType::Enum;     \
        static constexpr const char *name = Prefix##Event::name;       \
    };

TAS_SIMPLE_GAME_EVENTS(DEFINE_SIMPLE_EVENT)
#undef DEFINE_SIMPLE_EVENT

// ============================================================================
// ExitGameEvent (no legacy name, not forwarded to Lua)
// ============================================================================
struct ExitGameEvent {};

// ============================================================================
// Data-carrying events (defined manually)
// ============================================================================

struct PreCheckpointReachedEvent {
    static constexpr const char *name = "pre_checkpoint_reached";
    int sector = -1;
};
template <>
struct EventTypeTraits<PreCheckpointReachedEvent> {
    static constexpr GameEventType type = GameEventType::PreCheckpointReached;
    static constexpr const char *name = PreCheckpointReachedEvent::name;
};

struct PostCheckpointReachedEvent {
    static constexpr const char *name = "post_checkpoint_reached";
    int sector = -1;
};
template <>
struct EventTypeTraits<PostCheckpointReachedEvent> {
    static constexpr GameEventType type = GameEventType::PostCheckpointReached;
    static constexpr const char *name = PostCheckpointReachedEvent::name;
};

struct ExtraPointEvent {
    static constexpr const char *name = "extra_point";
    int points = 0;
};
template <>
struct EventTypeTraits<ExtraPointEvent> {
    static constexpr GameEventType type = GameEventType::ExtraPoint;
    static constexpr const char *name = ExtraPointEvent::name;
};

struct PreSubLifeEvent {
    static constexpr const char *name = "pre_sub_life";
    int lifeCount = 0;
};
template <>
struct EventTypeTraits<PreSubLifeEvent> {
    static constexpr GameEventType type = GameEventType::PreSubLife;
    static constexpr const char *name = PreSubLifeEvent::name;
};

struct PostSubLifeEvent {
    static constexpr const char *name = "post_sub_life";
    int lifeCount = 0;
};
template <>
struct EventTypeTraits<PostSubLifeEvent> {
    static constexpr GameEventType type = GameEventType::PostSubLife;
    static constexpr const char *name = PostSubLifeEvent::name;
};

struct PreLifeUpEvent {
    static constexpr const char *name = "pre_life_up";
    int lifeCount = 0;
};
template <>
struct EventTypeTraits<PreLifeUpEvent> {
    static constexpr GameEventType type = GameEventType::PreLifeUp;
    static constexpr const char *name = PreLifeUpEvent::name;
};

struct PostLifeUpEvent {
    static constexpr const char *name = "post_life_up";
    int lifeCount = 0;
};
template <>
struct EventTypeTraits<PostLifeUpEvent> {
    static constexpr GameEventType type = GameEventType::PostLifeUp;
    static constexpr const char *name = PostLifeUpEvent::name;
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

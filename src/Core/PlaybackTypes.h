#pragma once

/**
 * @file PlaybackTypes.h
 * @brief Shared playback type definitions used across module boundaries.
 *
 * Extracted from TASControllers.h so that TASEngine.h (and other headers)
 * can reference PlaybackType without pulling in the full controller /
 * strategy / project / Lua runtime header chain.
 */

/**
 * @enum PlaybackType
 * @brief Identifies the active playback mode.
 */
enum class PlaybackType {
    None,   // No playback active
    Script, // Lua script playback via ScriptContextManager
    Record  // Binary record playback via RecordPlayer
};

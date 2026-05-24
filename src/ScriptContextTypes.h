#pragma once

/**
 * @file ScriptContextTypes.h
 * @brief Shared type definitions for the script context system.
 *
 * This header is intentionally lightweight — it contains only enum definitions
 * used across module boundaries (e.g., by ScriptContextManager, ScriptContext,
 * LuaApi bindings) so that consumers don't need to include the full
 * ScriptContext.h and its runtime dependencies.
 */

/**
 * @brief Type of script context
 */
enum class ScriptContextType {
    Global, // Global context that persists across levels
    Level,  // Level-specific context
    Custom  // User-created context
};

/**
 * @brief Lua GC mode (Lua 5.4+)
 */
enum class LuaGCMode {
    Generational, // Generational GC (default for TAS, better for short-burst workloads)
    Incremental   // Incremental GC (better for long-lived scripts with timely finalization)
};

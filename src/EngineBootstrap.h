#pragma once

/**
 * @file EngineBootstrap.h
 * @brief Composition root for TASEngine subsystem wiring.
 *
 * EngineBootstrap separates the "what to create and how to wire it"
 * concern from TASEngine's runtime coordination logic. This keeps
 * TASEngine.h free of subsystem-specific includes and makes the
 * dependency graph explicit in one place.
 *
 * Adding a new subsystem only requires changes here and in the
 * corresponding module — TASEngine itself remains untouched.
 */

class TASEngine;

/**
 * @class EngineBootstrap
 * @brief Creates and initialises all TASEngine subsystems.
 *
 * All subsystem construction and wiring lives here. TASEngine delegates to
 * EngineBootstrap during Initialize() / Shutdown() so engine.h does not need
 * to know about every concrete subsystem type.
 */
class EngineBootstrap {
public:
    EngineBootstrap() = delete;

    /**
     * @brief Wire up the runtime core and service graph.
     *
     * This covers:
     *  1. Core subsystem creation (InputSystem, EventManager, Recorder, ...)
     *  2. State machine and service initialisation
     *  3. Savestate and runtime router creation
     *
     * @param engine The TASEngine instance to initialise.
     * @return true on success, false on failure (errors are logged).
     */
    static bool InitializeCoreSubsystems(TASEngine &engine);

    /**
     * @brief Initialise higher-level subsystems that depend on core wiring.
     *
     * This covers:
     *  1. ScriptContextManager initialisation
     *  2. REPL server (if enabled)
     *  3. StartupProjectManager
     *  4. Typed runtime event components
     *
     * @param engine The TASEngine instance.
     * @return true on success, false on failure.
     */
    static bool InitializeHighLevelSubsystems(TASEngine &engine);
};

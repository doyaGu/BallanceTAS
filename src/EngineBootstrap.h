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
 * @brief Creates, registers, and initialises all TASEngine subsystems.
 *
 * All subsystem construction and ServiceContainer registration lives here.
 * TASEngine delegates to EngineBootstrap during Initialize() / Shutdown()
 * so that engine.h does not need to know about every concrete subsystem type.
 */
class EngineBootstrap {
public:
    EngineBootstrap() = delete;

    /**
     * @brief Wire up all subsystems into the engine's ServiceContainer.
     *
     * This covers:
     *  1. ServiceContainer creation and external‐dependency registration
     *  2. Core subsystem creation (InputSystem, EventManager, Recorder, …)
     *  3. State machine and controller initialisation
     *  4. SavestateManager creation
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
     *  3. ProjectManager
     *  4. StartupProjectManager
     *  5. Status callbacks
     *
     * @param engine The TASEngine instance.
     * @return true on success, false on failure.
     */
    static bool InitializeHighLevelSubsystems(TASEngine &engine);
};

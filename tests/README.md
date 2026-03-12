# BallanceTAS Test Suite

This directory contains both **standalone** and **full integration** tests for
BallanceTAS.  Standalone tests can be built and run on *any* machine without
VirtoolsSDK or BML installed.

## Directory Layout

```
tests/
├── README.md                      # This file
├── CMakeLists.txt                 # Test build config (GoogleTest, CTest)
│
│   ── C++ standalone tests (no SDK) ──────────────────────────────
├── ResultTest.cpp                 # Result<T> error-handling tests
├── StateMachineTest.cpp           # TASStateMachine state-transition tests
├── LoggerTest.cpp                 # Logger + ILogSink decoupling tests
├── ResourceManagerTest.cpp        # RAII / temp-file management tests
├── ConsoleLogSink.h               # ILogSink → stdout (used in tests)
│
│   ── C++ integration tests (require BUILD_MOD) ─────────────────
├── LuaApiTest.cpp                 # Full Lua API tests against real engine
│
│   ── Lua standalone tests (mock harness, no game) ──────────────
├── mock_tas_api.lua               # Mock `tas` namespace for standalone use
├── run_standalone_tests.lua       # Standalone Lua runner (exit-code aware)
├── test_core.lua                  # Core API integration tests
├── test_savestate.lua             # Savestate integration tests
│
│   ── Lua in-game runner ────────────────────────────────────────
└── run_all_tests.lua              # In-game runner (uses real game APIs)
```

## Quick Start

### 1. Build & run standalone C++ tests (no SDK)

```bash
# Configure — SDK not required
cmake -B build-tests -DBUILD_MOD=OFF -DBUILD_TESTS=ON

# Build only test targets
cmake --build build-tests --target ResultTest StateMachineTest LoggerTest ResourceManagerTest

# Run via CTest
cd build-tests && ctest --output-on-failure
```

### 2. Run standalone Lua tests

Requires a Lua 5.4 interpreter on `PATH`:

```bash
lua tests/run_standalone_tests.lua
```

Or, if CTest found the interpreter during configure:

```bash
cd build-tests && ctest -R LuaStandalone --output-on-failure
```

### 3. Build & run full integration tests (SDK required)

```bash
cmake -B build -DBUILD_MOD=ON -DBUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

## Architecture

### Decoupled core (`tas_core`)

The `tas_core` static library is **pure C++** with zero SDK dependencies:

| Component               | Contents                                        |
|-------------------------|-------------------------------------------------|
| `Result.h`              | Rust-style Result<T> monad                      |
| `TASStateMachine`       | State machine with handler interface             |
| `Logger` / `ILogSink`   | Logging facade + pluggable sink abstraction      |
| `ResourceManager`       | RAII temp-file & cleanup management              |
| `ServiceContainer`      | Lightweight IoC container                        |

Because `tas_core` does not link BML, test executables that only need
these components can compile and link without any game SDKs.

### Logger decoupling

`Logger.cpp` uses `ILogSink*` (pure C++ interface) rather than BML's
`ILogger*` directly.  Two concrete sinks are provided:

* **`BMLLogSink`** (in `src/`) — adapter for the real BML logger, used by the
  mod at runtime.
* **`ConsoleLogSink`** (in `tests/`) — prints to stdout/stderr, used in tests.

### Lua mock harness

`mock_tas_api.lua` creates a global `tas` table that stubs every API used by
`test_core.lua` and `test_savestate.lua`:

* Timing → synthetic tick/frame counter
* Logging → stdout via `print()`
* Savestate → in-memory table
* Level/ball → dummy data
* `tas._mock.*` — control functions to reset state between tests

## Writing New Tests

### C++ standalone test

1. Create `tests/MyTest.cpp` with GoogleTest macros.
2. In `tests/CMakeLists.txt`:
   ```cmake
   add_tas_test(MyTest
       SOURCES MyTest.cpp
       DEPENDENCIES tas_core    # add more if needed
   )
   add_test(NAME MyTest COMMAND MyTest)
   ```
3. Build & run.

### Lua standalone test

1. Create `tests/test_myfeature.lua` following the `run_test()` / `return { run = main }` pattern.
2. Add the path to `test_files` in `run_standalone_tests.lua`.
3. Add any new API stubs needed to `mock_tas_api.lua`.
4. Run: `lua tests/run_standalone_tests.lua`.

## CI / Headless

A CI pipeline only needs CMake, a C++20 compiler, and (optionally) Lua 5.4:

```yaml
steps:
  - name: Configure (standalone)
    run: cmake -B build -DBUILD_MOD=OFF -DBUILD_TESTS=ON

  - name: Build tests
    run: cmake --build build

  - name: Run C++ tests
    run: cd build && ctest --output-on-failure

  - name: Run Lua tests
    run: lua tests/run_standalone_tests.lua
```

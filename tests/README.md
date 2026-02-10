# BallanceTAS Integration Test Suite

This directory contains comprehensive integration tests for BallanceTAS functionality.

## Test Structure

```
tests/
├── README.md                    # This file
├── test_core.lua                # Core API tests
├── test_input.lua               # Input system tests
├── test_async.lua               # Async/concurrency tests
├── test_recording.lua           # Recording API tests
├── test_savestate.lua           # Savestate system tests
├── test_context_comm.lua        # Context communication tests
├── test_rng.lua                 # RNG determinism tests
├── test_integration.lua         # Full integration test
└── run_all_tests.lua            # Test runner
```

## Running Tests

### Run All Tests
```lua
-- Load and run all tests
dofile("tests/run_all_tests.lua")
```

### Run Individual Test
```lua
-- Run specific test
dofile("tests/test_core.lua")
```

## Test Categories

### 1. Core API Tests (`test_core.lua`)
- Logging functions
- Tick/frame counting
- Manifest access
- Error handling

### 2. Input Tests (`test_input.lua`)
- Key press/hold/release
- Input state queries
- Input timing accuracy

### 3. Async Tests (`test_async.lua`)
- async/await patterns
- Coroutine management
- Parallel/race/all operations
- Timeout and retry

### 4. Recording Tests (`test_recording.lua`)
- Frame recording and playback
- Section/Marker/Comment management
- Macro creation and application
- Branch and snapshot operations
- Undo/Redo functionality

### 5. Savestate Tests (`test_savestate.lua`)
- State save and load
- State validation
- Cross-level compatibility checks
- RNG state preservation

### 6. Context Communication Tests (`test_context_comm.lua`)
- SharedData operations
- MessageBus pub/sub
- Context event handling
- Cross-context coordination

### 7. RNG Tests (`test_rng.lua`)
- Determinism verification
- State save/restore
- Seed consistency
- Call counting

### 8. Integration Tests (`test_integration.lua`)
- Multi-paradigm workflows
- Complete TAS scenarios
- Error recovery
- Performance benchmarks

## Test Framework

Tests use a simple assert-based framework:

```lua
-- Test template
function test_feature_name()
    -- Setup
    local initial_value = 0

    -- Execute
    local result = some_function(initial_value)

    -- Verify
    assert(result == expected_value, "Test failed: expected " .. expected_value)

    -- Cleanup (if needed)
    cleanup_function()

    return true  -- Test passed
end
```

## Test Results

Each test outputs:
- ✅ Pass: Test completed successfully
- ❌ Fail: Test failed with error message
- ⏭️ Skip: Test skipped (feature not available)

## Writing New Tests

1. Create test file: `test_<feature>.lua`
2. Implement test functions
3. Add to `run_all_tests.lua`
4. Document in this README

## Continuous Testing

For CI/CD integration, run:
```bash
# Run headless test suite
ballance_tas --headless --script tests/run_all_tests.lua
```

## Troubleshooting

### Test Failures
- Check log output for detailed error messages
- Verify TASEngine is initialized
- Ensure level is loaded before level-specific tests

### Flaky Tests
- Some tests may be timing-sensitive
- Run multiple times to verify consistency
- Check for race conditions in async tests

## Coverage

Current test coverage (estimated):
- Core API: 90%
- Input System: 85%
- Async/Concurrency: 80%
- Recording: 75%
- Savestate: 70%
- Context Communication: 85%
- RNG: 90%

## Contributing

When adding new features:
1. Add corresponding tests
2. Update this README
3. Run full test suite before committing
4. Maintain >80% coverage target

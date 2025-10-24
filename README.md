# BallanceTAS

A Tool-Assisted Speedrun (TAS) framework for Ballance, providing comprehensive recording, generation, debugging, and execution capabilities through a sophisticated Lua-based scripting system.

## Overview

BallanceTAS is a framework that enables creators to develop precise, deterministic, and reproducible tool-assisted speedruns and puzzle solutions for Ballance. It bridges the gap between manual recording and precise scripting, offering both automated recording capabilities and a powerful manual scripting API with frame-perfect control at 132Hz.

## Features

### Core TAS Functionality
- **Deterministic Execution**: 100% reproducible frame-precise control at 132Hz
- **Lua Scripting Engine**: Complete Lua 5.4.8 integration with extensive APIs
- **Smart Recording System**: Converts gameplay input into structured Lua scripts
- **Real-time Debugging**: Comprehensive in-game OSD and remote REPL server

### Advanced API Categories
- **Input Control**: Keyboard, mouse, joystick with frame-precise timing
- **World Query**: Real-time game state, ball physics, object manipulation
- **Concurrency & Events**: Sophisticated coroutine system with event-driven programming
- **Mathematical Types**: Complete VxVector, VxQuaternion, VxMatrix, etc. bindings
- **Game Object Access**: Full CK3dEntity, CKCamera, PhysicsObject APIs
- **Record Playback**: Advanced TAS editing and frame manipulation tools

## Requirements

### System Requirements
- Windows 10 or later
- Ballance game installation
- BML+ (Ballance Mod Loader Plus) installed

### Development Requirements
- Modern C++ compiler with C++20 support (Visual Studio 2019/2022 recommended)
- CMake 3.14 or later
- Git

### Dependencies
- VirtoolsSDK
- BML+ (Ballance Mod Loader Plus)
- Lua 5.4.8 (embedded)
- MinHook (function hooking)
- sol2 (C++/Lua bindings)
- fmt (modern C++ formatting)
- zip (compression)

## Installation

### Building from Source

```bash
# Clone the repository
git clone <repository-url>
cd BallanceTAS

# Configure with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build build --config Release

# Install (default: ./install directory)
cmake --install build
```

### Setting Up the Framework

1. Ensure Ballance game with BML mod loader is installed
2. Place the compiled `BallanceTAS.bmodp` in the BML plugins directory
3. Launch Ballance game
4. Access TAS functionality through the available interfaces

## Quick Start

### Accessing TAS Features
- **TAS Menu**: Press F10 in the main menu
- **In-Game OSD**: Press F11 during gameplay
- **REPL Server**: Connect to the remote debugging interface

### Basic Workflow
1. **Record**: Use F10 menu to record gameplay → generates structured Lua script
2. **Edit**: Modify generated scripts with full IntelliSense support
3. **Debug**: Use F11 OSD and remote REPL for real-time debugging
4. **Validate**: Use `tas.assert()` for deterministic verification
5. **Execute**: Play back refined scripts through TAS menu

### Project Structure
```
/ModLoader/TAS/MyAwesomeTAS/
├── manifest.lua       -- Project metadata
└── main.lua           -- Entry script
```

## Usage Examples

### Basic Lua Script Example
```lua
-- Simple movement script
function main()
    -- Move forward for 120 frames
    tas.keyboard.press("W", 120)

    -- Jump at frame 60
    tas.keyboard.key("Space", 60, 1)

    -- Wait for ball to stabilize
    while not tas.ball.is_stable() do
        tas.wait(1)
    end

    tas.log("Movement complete!")
end
```

### World Query Example
```lua
function main()
    -- Get current ball position and velocity
    local pos = tas.ball.position()
    local vel = tas.ball.velocity()

    tas.log(string.format("Position: %.2f, %.2f, %.2f",
                           pos.x, pos.y, pos.z))
    tas.log(string.format("Velocity: %.2f", vel:length()))

    -- Wait for ball to reach certain height
    while pos.y < 50 do
        pos = tas.ball.position()
        tas.wait(1)
    end

    tas.log("Target height reached!")
end
```

## Architecture

The system follows a 6-layered architecture:

1. **User Interface Layer**: ImGui-based UI and in-game OSD
2. **Scripting Layer**: Lua scripts with coroutine-based execution
3. **Host API Layer**: sol2 bindings for C++/Lua integration
4. **Host Core Layer**: C++ BML mod with core framework logic
5. **Hooking Layer**: MinHook-based function interception
6. **Game Layer**: Ballance game with Virtools/CK physics engine

### Core Components
- **TASEngine**: Central coordinator and execution engine
- **InputSystem**: Input synthesizer and virtual keyboard state management
- **GameInterface**: Safe game world information provider
- **LuaScheduler**: Advanced coroutine management system
- **Recorder & ScriptGenerator**: Smart recording-to-script pipeline
- **UIManager**: Main UI and In-Game OSD management

## Contributing

### Development Guidelines
- Use modern C++20 features and best practices
- Follow existing code style and naming conventions
- Add comprehensive documentation for new APIs
- Include tests for new functionality
- Ensure deterministic behavior and frame-perfect precision

### Code Structure
- **src/**: Main C++ source code
- **deps/**: External dependencies
- **TASEditor/**: TAS editing tools and utilities
- **tests/**: Test infrastructure and API tests

### Build System
- CMake configuration with platform-specific optimizations
- Dependency management with submodule support
- Automatic versioning and build metadata

## License

This project is licensed under the MIT license - see the LICENSE file for details.

## Credits

- **Ballance**: Original game developed by Cyparade (2004)
- **BML+**: Ballance Mod Loader Plus framework
- **Virtools**: Game engine and physics system
- **Lua**: Scripting language and runtime
- **MinHook**: Function hooking library

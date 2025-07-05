# CMakeLists.txt for Lua
# Cross-platform build system
# Supports all platforms from original Makefile: guess aix bsd c89 freebsd generic ios linux macosx mingw posix solaris

cmake_minimum_required(VERSION 3.12)

# Project definition
project(lua
        VERSION 5.4.8
        DESCRIPTION "Lua programming language"
        HOMEPAGE_URL "https://www.lua.org"
        LANGUAGES C
)

# Policy settings for modern CMake
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
endif()

if(POLICY CMP0075)
    cmake_policy(SET CMP0075 NEW)  # Include file check macros honor CMAKE_REQUIRED_LIBRARIES
endif()

# Include standard modules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(CheckCSourceCompiles)

# Build options
option(LUA_BUILD_SHARED "Build shared library" OFF)
option(LUA_BUILD_STATIC "Build static library" ON)
option(LUA_BUILD_READLINE "Build with readline support (Unix only)" OFF)
option(LUA_BUILD_LIBEDIT "Build with libedit support (alternative to readline)" OFF)
option(LUA_BUILD_TESTS "Build test suite" OFF)
option(LUA_ENABLE_LTO "Enable Link Time Optimization" OFF)
option(LUA_FORCE_C89 "Force C89 standard compliance" OFF)

# Platform detection and configuration
set(LUA_PLATFORM "guess" CACHE STRING "Target platform (guess|aix|bsd|c89|freebsd|generic|ios|linux|macosx|mingw|posix|solaris)")
set_property(CACHE LUA_PLATFORM PROPERTY STRINGS "guess;aix;bsd;c89;freebsd;generic;ios;linux;macosx;mingw;posix;solaris")

# Auto-detect platform if set to 'guess'
if(LUA_PLATFORM STREQUAL "guess")
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(LUA_PLATFORM "linux")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        if(CMAKE_OSX_SYSROOT MATCHES "iphone")
            set(LUA_PLATFORM "ios")
        else()
            set(LUA_PLATFORM "macosx")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        if(MINGW)
            set(LUA_PLATFORM "mingw")
        else()
            # For MSVC and other Windows compilers, use generic
            # LUA_USE_WINDOWS will be auto-detected from _WIN32
            set(LUA_PLATFORM "generic")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        set(LUA_PLATFORM "freebsd")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
        set(LUA_PLATFORM "bsd")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "AIX")
        set(LUA_PLATFORM "aix")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
        set(LUA_PLATFORM "solaris")
    elseif(UNIX)
        set(LUA_PLATFORM "posix")
    else()
        set(LUA_PLATFORM "generic")
    endif()
endif()

message(STATUS "Lua target platform: ${LUA_PLATFORM}")

# C standard configuration
if(LUA_FORCE_C89 OR LUA_PLATFORM STREQUAL "c89")
    set(CMAKE_C_STANDARD 90)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
else()
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
endif()

# Compiler-specific settings
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(LUA_COMPILER_FLAGS -Wall -Wextra -Wdeclaration-after-statement)
    if(LUA_ENABLE_LTO)
        list(APPEND LUA_COMPILER_FLAGS -flto)
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(LUA_COMPILER_FLAGS -Wall -Wextra -Wdeclaration-after-statement)
    if(LUA_ENABLE_LTO)
        list(APPEND LUA_COMPILER_FLAGS -flto)
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(LUA_COMPILER_FLAGS /W3)
    if(LUA_ENABLE_LTO)
        list(APPEND LUA_COMPILER_FLAGS /GL)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LTCG")
    endif()
elseif(CMAKE_C_COMPILER_ID STREQUAL "Intel")
    set(LUA_COMPILER_FLAGS -Wall)
endif()

# Platform-specific compile definitions and link libraries
set(LUA_COMPILE_DEFINITIONS "")
set(LUA_LINK_LIBRARIES "")

# Check for system capabilities
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("readline/readline.h" HAVE_READLINE_H)
check_include_file("editline/readline.h" HAVE_EDITLINE_H)
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_library_exists(m sin "" HAVE_LIBM)
check_library_exists(dl dlopen "" HAVE_LIBDL)
check_library_exists(readline readline "" HAVE_LIBREADLINE)
check_library_exists(edit readline "" HAVE_LIBEDIT)

# Platform-specific configurations
if(LUA_PLATFORM STREQUAL "linux")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_LINUX)
    if(HAVE_LIBDL)
        list(APPEND LUA_LINK_LIBRARIES dl)
    endif()
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "macosx")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_MACOSX)
    # macOS has dlopen built-in, no -ldl needed
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "ios")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_IOS)
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "freebsd")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_LINUX)  # FreeBSD uses same as Linux
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "bsd")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_POSIX)
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "aix")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_POSIX _ALL_SOURCE)
    if(HAVE_LIBDL)
        list(APPEND LUA_LINK_LIBRARIES dl)
    endif()
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "solaris")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_POSIX)
    if(HAVE_LIBDL)
        list(APPEND LUA_LINK_LIBRARIES dl)
    endif()
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()
    # Solaris needs socket and nsl for network functions
    list(APPEND LUA_LINK_LIBRARIES socket nsl)

elseif(LUA_PLATFORM STREQUAL "mingw")
    # Don't define LUA_USE_WINDOWS - it's auto-detected from _WIN32 in luaconf.h
    # Windows/MinGW doesn't need additional libraries for basic functionality

elseif(LUA_PLATFORM STREQUAL "posix")
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_POSIX)
    if(HAVE_LIBDL)
        list(APPEND LUA_LINK_LIBRARIES dl)
    endif()
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

elseif(LUA_PLATFORM STREQUAL "c89")
    # Force C89 compatibility mode
    list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_C89)
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()

else() # generic
    # Minimal configuration, no platform-specific features
    if(HAVE_LIBM)
        list(APPEND LUA_LINK_LIBRARIES m)
    endif()
endif()

# Readline/Libedit support
if(LUA_BUILD_READLINE AND NOT LUA_PLATFORM STREQUAL "mingw" AND NOT LUA_PLATFORM STREQUAL "generic")
    if(HAVE_LIBREADLINE AND HAVE_READLINE_H)
        list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_READLINE)
        list(APPEND LUA_LINK_LIBRARIES readline)
        message(STATUS "Readline support enabled")
    elseif(LUA_BUILD_LIBEDIT AND HAVE_LIBEDIT AND HAVE_EDITLINE_H)
        list(APPEND LUA_COMPILE_DEFINITIONS LUA_USE_READLINE)
        list(APPEND LUA_LINK_LIBRARIES edit)
        message(STATUS "Libedit support enabled")
    else()
        message(WARNING "Readline requested but not found")
    endif()
endif()

# Source files for liblua
set(LUA_LIB_SOURCES
        src/lapi.c
        src/lcode.c
        src/lctype.c
        src/ldebug.c
        src/ldo.c
        src/ldump.c
        src/lfunc.c
        src/lgc.c
        src/llex.c
        src/lmem.c
        src/lobject.c
        src/lopcodes.c
        src/lparser.c
        src/lstate.c
        src/lstring.c
        src/ltable.c
        src/ltm.c
        src/lundump.c
        src/lvm.c
        src/lzio.c
        src/lauxlib.c
        src/lbaselib.c
        src/lcorolib.c
        src/ldblib.c
        src/liolib.c
        src/lmathlib.c
        src/loadlib.c
        src/loslib.c
        src/lstrlib.c
        src/ltablib.c
        src/lutf8lib.c
        src/linit.c
)

# Windows resource files for DLL version information
set(LUA_WIN_RESOURCES "")
if(WIN32)
    # Enable RC language for resource compilation
    enable_language(RC OPTIONAL)

    # Check if RC compiler is available
    if(CMAKE_RC_COMPILER)
        message(STATUS "Windows RC compiler found: ${CMAKE_RC_COMPILER}")

        # Extract version and copyright information from lua.h
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.h")
            file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/lua.h" LUA_H_CONTENT)

            # Extract copyright information
            string(REGEX MATCH "#define LUA_COPYRIGHT[[:space:]]+\"([^\"]+)\""
                    LUA_COPYRIGHT_MATCH "${LUA_H_CONTENT}")
            if(LUA_COPYRIGHT_MATCH)
                string(REGEX REPLACE "#define LUA_COPYRIGHT[[:space:]]+\"([^\"]+)\"" "\\1"
                        LUA_COPYRIGHT_FROM_HEADER "${LUA_COPYRIGHT_MATCH}")
            else()
                set(LUA_COPYRIGHT_FROM_HEADER "Copyright (C) 1994-2025 Lua.org, PUC-Rio")
            endif()

            # Extract authors information
            string(REGEX MATCH "#define LUA_AUTHORS[[:space:]]+\"([^\"]+)\""
                    LUA_AUTHORS_MATCH "${LUA_H_CONTENT}")
            if(LUA_AUTHORS_MATCH)
                string(REGEX REPLACE "#define LUA_AUTHORS[[:space:]]+\"([^\"]+)\"" "\\1"
                        LUA_AUTHORS_FROM_HEADER "${LUA_AUTHORS_MATCH}")
            else()
                set(LUA_AUTHORS_FROM_HEADER "R. Ierusalimschy, L. H. de Figueiredo, W. Celes")
            endif()

            # Extract release information
            string(REGEX MATCH "#define LUA_RELEASE[[:space:]]+\"([^\"]+)\""
                    LUA_RELEASE_MATCH "${LUA_H_CONTENT}")
            if(LUA_RELEASE_MATCH)
                string(REGEX REPLACE "#define LUA_RELEASE[[:space:]]+\"([^\"]+)\"" "\\1"
                        LUA_RELEASE_FROM_HEADER "${LUA_RELEASE_MATCH}")
            else()
                set(LUA_RELEASE_FROM_HEADER "Lua ${PROJECT_VERSION}")
            endif()

            message(STATUS "Extracted from lua.h:")
            message(STATUS "  Copyright: ${LUA_COPYRIGHT_FROM_HEADER}")
            message(STATUS "  Authors: ${LUA_AUTHORS_FROM_HEADER}")
            message(STATUS "  Release: ${LUA_RELEASE_FROM_HEADER}")
        else()
            # Fallback values if lua.h is not found
            set(LUA_COPYRIGHT_FROM_HEADER "Copyright (C) 1994-2025 Lua.org, PUC-Rio")
            set(LUA_AUTHORS_FROM_HEADER "R. Ierusalimschy, L. H. de Figueiredo, W. Celes")
            set(LUA_RELEASE_FROM_HEADER "Lua ${PROJECT_VERSION}")
            message(WARNING "lua.h not found, using fallback copyright information")
        endif()

        # Configure version information template
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lua_version.rc.in")
            configure_file(
                    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lua_version.rc.in"
                    "${CMAKE_CURRENT_BINARY_DIR}/lua_version.rc"
                    @ONLY
            )
            set(LUA_WIN_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/lua_version.rc")
        endif()
    else()
        message(STATUS "Windows RC compiler not found - version info will not be embedded")
    endif()
endif()

# Headers to install
set(LUA_PUBLIC_HEADERS
        src/lua.h
        src/luaconf.h
        src/lualib.h
        src/lauxlib.h
        src/lua.hpp
)

# Function to setup common target properties
function(setup_lua_target target_name)
    target_compile_definitions(${target_name} PRIVATE ${LUA_COMPILE_DEFINITIONS})
    target_compile_options(${target_name} PRIVATE ${LUA_COMPILER_FLAGS})

    if(LUA_LINK_LIBRARIES)
        target_link_libraries(${target_name} PUBLIC ${LUA_LINK_LIBRARIES})
    endif()

    target_include_directories(${target_name}
            PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # Platform-specific properties
    if(WIN32)
        # Disable MSVC security warnings - let luaconf.h handle platform detection
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()

    # Large file support on 32-bit systems
    if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND UNIX)
        target_compile_definitions(${target_name} PRIVATE _FILE_OFFSET_BITS=64)
    endif()
endfunction()

# Create static library
if(LUA_BUILD_STATIC)
    add_library(liblua_static STATIC ${LUA_LIB_SOURCES})
    setup_lua_target(liblua_static)

    set_target_properties(liblua_static PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
            POSITION_INDEPENDENT_CODE ON
    )

    # Set output name based on whether shared library is also being built
    if(LUA_BUILD_SHARED)
        # When building both, differentiate the static library name
        if(WIN32)
            set_target_properties(liblua_static PROPERTIES OUTPUT_NAME lua-static)
        else()
            set_target_properties(liblua_static PROPERTIES OUTPUT_NAME lua-static)
        endif()
    else()
        # When building only static, use the simple name
        set_target_properties(liblua_static PROPERTIES OUTPUT_NAME lua)
    endif()

    add_library(lua::lua_static ALIAS liblua_static)
endif()

# Create shared library
if(LUA_BUILD_SHARED)
    add_library(liblua_shared SHARED ${LUA_LIB_SOURCES})

    # Add Windows resources to shared library
    if(WIN32 AND CMAKE_RC_COMPILER AND LUA_WIN_RESOURCES)
        target_sources(liblua_shared PRIVATE ${LUA_WIN_RESOURCES})
    endif()

    setup_lua_target(liblua_shared)

    set_target_properties(liblua_shared PROPERTIES
            OUTPUT_NAME lua
            SOVERSION ${PROJECT_VERSION_MAJOR}
            VERSION ${PROJECT_VERSION}
            LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Windows DLL export handling - let luaconf.h auto-detect everything
    if(WIN32)
        target_compile_definitions(liblua_shared PRIVATE LUA_BUILD_AS_DLL)
        target_compile_definitions(liblua_shared INTERFACE LUA_BUILD_AS_DLL)

        # Enable automatic symbol export
        set_target_properties(liblua_shared PROPERTIES
                WINDOWS_EXPORT_ALL_SYMBOLS ON
        )
    endif()

    add_library(lua::lua_shared ALIAS liblua_shared)
endif()

# Choose default library target
if(LUA_BUILD_SHARED)
    set(LUA_TARGET liblua_shared)
    add_library(lua::lua ALIAS liblua_shared)
else()
    set(LUA_TARGET liblua_static)
    add_library(lua::lua ALIAS liblua_static)
endif()

# Create lua executable
add_executable(lua src/lua.c)
target_link_libraries(lua PRIVATE ${LUA_TARGET})
set_target_properties(lua PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add Windows resources to lua executable
if(WIN32 AND CMAKE_RC_COMPILER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lua_exe.rc.in")
        # Configure with the same variables extracted from lua.h
        configure_file(
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lua_exe.rc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/lua_exe.rc"
                @ONLY
        )
        target_sources(lua PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/lua_exe.rc")
    endif()
endif()

# Create luac executable
add_executable(luac src/luac.c)
target_link_libraries(luac PRIVATE ${LUA_TARGET})
set_target_properties(luac PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add Windows resources to luac executable
if(WIN32 AND CMAKE_RC_COMPILER)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/luac_exe.rc.in")
        # Configure with the same variables extracted from lua.h
        configure_file(
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/luac_exe.rc.in"
                "${CMAKE_CURRENT_BINARY_DIR}/luac_exe.rc"
                @ONLY
        )
        target_sources(luac PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/luac_exe.rc")
    endif()
endif()

# Optional test suite
if(LUA_BUILD_TESTS)
    enable_testing()

    # Basic functionality test
    add_test(NAME lua_version
            COMMAND lua -v
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Lua compiler test
    add_test(NAME luac_help
            COMMAND luac -h
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    # Simple Lua script test
    file(WRITE ${CMAKE_BINARY_DIR}/test_basic.lua "print('Hello, Lua!')")
    add_test(NAME lua_basic_script
            COMMAND lua test_basic.lua
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(lua_basic_script PROPERTIES
            PASS_REGULAR_EXPRESSION "Hello, Lua!"
    )
endif()

# Installation rules with component support
if(LUA_BUILD_STATIC)
    install(TARGETS liblua_static
            EXPORT luaTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT Development
    )
endif()

if(LUA_BUILD_SHARED)
    install(TARGETS liblua_shared
            EXPORT luaTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT Runtime
    )
endif()

install(TARGETS lua luac
        EXPORT luaTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Runtime
)

# Install headers
install(FILES ${LUA_PUBLIC_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT Development
)

# Install man pages if they exist
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/lua.1")
    install(FILES doc/lua.1 doc/luac.1
            DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
            COMPONENT Documentation
    )
endif()

# Create directories for Lua modules
install(DIRECTORY DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/lua/${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        COMPONENT Runtime
)
install(DIRECTORY DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/lua/${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        COMPONENT Runtime
)

# Export targets
install(EXPORT luaTargets
        FILE luaTargets.cmake
        NAMESPACE lua::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lua
        COMPONENT Development
)

# Create config file
configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/luaConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/luaConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lua
        PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

# Create version file
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/luaConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/luaConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/luaConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lua
        COMPONENT Development
)

# Create pkg-config file
configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/lua.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/lua.pc"
        @ONLY
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/lua.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        COMPONENT Development
)

# Custom targets for convenience
add_custom_target(local
        COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/install
        -P ${CMAKE_BINARY_DIR}/cmake_install.cmake
        COMMENT "Installing to local directory"
)

# Echo configuration (equivalent to 'make echo')
add_custom_target(echo
        COMMAND ${CMAKE_COMMAND} -E echo "=== Lua Build Configuration ==="
        COMMAND ${CMAKE_COMMAND} -E echo "Platform: ${LUA_PLATFORM}"
        COMMAND ${CMAKE_COMMAND} -E echo "System: ${CMAKE_SYSTEM_NAME}"
        COMMAND ${CMAKE_COMMAND} -E echo "Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E echo "C Standard: C${CMAKE_C_STANDARD}"
        COMMAND ${CMAKE_COMMAND} -E echo "Version: ${PROJECT_VERSION}"
        COMMAND ${CMAKE_COMMAND} -E echo "Install prefix: ${CMAKE_INSTALL_PREFIX}"
        COMMAND ${CMAKE_COMMAND} -E echo "Bindir: ${CMAKE_INSTALL_FULL_BINDIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "Libdir: ${CMAKE_INSTALL_FULL_LIBDIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "Includedir: ${CMAKE_INSTALL_FULL_INCLUDEDIR}"
        COMMAND ${CMAKE_COMMAND} -E echo "Build shared: ${LUA_BUILD_SHARED}"
        COMMAND ${CMAKE_COMMAND} -E echo "Build static: ${LUA_BUILD_STATIC}"
        COMMAND ${CMAKE_COMMAND} -E echo "Readline support: ${LUA_BUILD_READLINE}"
        COMMAND ${CMAKE_COMMAND} -E echo "Libedit support: ${LUA_BUILD_LIBEDIT}"
        COMMAND ${CMAKE_COMMAND} -E echo "Link Time Optimization: ${LUA_ENABLE_LTO}"
        COMMAND ${CMAKE_COMMAND} -E echo "Compile definitions: ${LUA_COMPILE_DEFINITIONS}"
        COMMAND ${CMAKE_COMMAND} -E echo "Link libraries: ${LUA_LINK_LIBRARIES}"
        COMMENT "Displaying build configuration"
)

# CPack configuration for packaging
set(CPACK_PACKAGE_NAME "lua")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "Lua.org")
set(CPACK_PACKAGE_CONTACT "info@lua.org")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/doc/readme.html")

# Platform-specific packaging
if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ;DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

# Component descriptions
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Lua Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Lua interpreter and compiler executables")
set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Lua Development")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Headers and libraries for Lua development")
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Lua Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "Manual pages and documentation")

include(CPack)

# Summary
message(STATUS "")
message(STATUS "=== Lua Configuration Summary ===")
message(STATUS "Platform: ${LUA_PLATFORM}")
message(STATUS "C Standard: C${CMAKE_C_STANDARD}")
message(STATUS "Shared library: ${LUA_BUILD_SHARED}")
message(STATUS "Static library: ${LUA_BUILD_STATIC}")
message(STATUS "Readline support: ${LUA_BUILD_READLINE}")
message(STATUS "Tests: ${LUA_BUILD_TESTS}")
message(STATUS "Link Time Optimization: ${LUA_ENABLE_LTO}")
if(WIN32 AND CMAKE_RC_COMPILER)
    message(STATUS "Windows version info: Enabled")
    if(LUA_COPYRIGHT_FROM_HEADER)
        message(STATUS "Copyright: ${LUA_COPYRIGHT_FROM_HEADER}")
    endif()
    if(LUA_AUTHORS_FROM_HEADER)
        message(STATUS "Authors: ${LUA_AUTHORS_FROM_HEADER}")
    endif()
else()
    message(STATUS "Windows version info: Not available")
endif()
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "================================")
message(STATUS "")
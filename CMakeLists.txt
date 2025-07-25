cmake_minimum_required(VERSION 3.14)

project(BallanceTAS
        VERSION 1.6.0
        LANGUAGES CXX
        DESCRIPTION "The TAS system for Ballance "
        HOMEPAGE_URL https://github.com/doyaGu/BallanceTAS)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED NO)
set(CMAKE_CXX_EXTENSIONS YES)

# Add path for custom modules
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(GenerateExportHeader)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Use folders to organize targets in an IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")

if (NOT WIN32)
    message(FATAL_ERROR "Only support Windows.")
endif ()

# Use relative paths
if (WIN32)
    set(CMAKE_SUPPRESS_REGENERATION TRUE)
endif ()

# Define build type if not specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Release' as no build type was specified")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type (Debug/Release)" FORCE)
    # Define standard build type options for GUI
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

# Set default install path
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}/install" CACHE PATH "The root directory of the installation" FORCE)
    message(STATUS "Setting default install directory to ${CMAKE_INSTALL_PREFIX} as no install directory was specified")
endif ()

# Disable msvc unsafe warnings globally
add_compile_definitions(
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_WARNINGS>
)

add_compile_options(
        $<$<CXX_COMPILER_ID:MSVC>:/Zc:char8_t->
)

set(TAS_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/include" CACHE INTERNAL "")
set(TAS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src" CACHE INTERNAL "")

find_package(VirtoolsSDK REQUIRED)
find_package(BML REQUIRED)

add_subdirectory(deps)
add_subdirectory(src)

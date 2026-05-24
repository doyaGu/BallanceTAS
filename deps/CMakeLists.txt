# Dependencies are usually required only when building the full mod (BUILD_MOD).
# Lua is also required by the standalone tas_lua_runtime tests.
if(BUILD_MOD OR BUILD_TESTS)

# lua
add_subdirectory(lua EXCLUDE_FROM_ALL)

endif()

if(BUILD_MOD)

# MinHook
add_subdirectory(minhook EXCLUDE_FROM_ALL)

# zip
add_subdirectory(zip EXCLUDE_FROM_ALL)

# fmt
add_subdirectory(fmt EXCLUDE_FROM_ALL)

# yyjson
add_subdirectory(yyjson EXCLUDE_FROM_ALL)

endif() # BUILD_MOD

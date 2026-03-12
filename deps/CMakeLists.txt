# Dependencies are only required when building the full mod (BUILD_MOD).
# Standalone test builds only need tas_core which is pure C++.
if(BUILD_MOD)

# MinHook
add_subdirectory(minhook EXCLUDE_FROM_ALL)

# lua
add_subdirectory(lua EXCLUDE_FROM_ALL)

# sol2
add_subdirectory(sol2 EXCLUDE_FROM_ALL)

# zip
add_subdirectory(zip EXCLUDE_FROM_ALL)

# fmt
add_subdirectory(fmt EXCLUDE_FROM_ALL)

# yyjson
add_subdirectory(yyjson EXCLUDE_FROM_ALL)

endif() # BUILD_MOD

# cglm
add_subdirectory (cglm)

# debugbreak
add_library (debug_break INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/debugbreak/debugbreak.h")
target_include_directories (debug_break INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/debugbreak")

# stb
add_library (stb INTERFACE)
target_include_directories (stb INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/stb")

# qsort
add_library (qsort INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/qsort/qsort.h")
target_include_directories (qsort INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/qsort")

# SDL3
set (SDL_SHARED ON CACHE BOOL "We use double-set with cache to override third party project options for good.")
set (SDL_SHARED ON)
add_subdirectory (SDL)

# SPIRV-Headers
add_subdirectory (SPIRV-Headers)

# Tracy
set (TRACY_ON_DEMAND ON)
add_subdirectory (tracy)

# volk
if (KAN_USE_VULKAN_API)
    add_subdirectory (volk)
endif ()

# VulkanMemoryAllocator
if (KAN_USE_VULKAN_API)
    add_subdirectory (VulkanMemoryAllocator)
endif ()

# xxHash
add_library (xxHash INTERFACE)
target_include_directories (xxHash INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/xxHash")

# re2c
# Hack to fix re2c build on Linux: it needs stdc++, but it is not linked to it by default.
# Due to this issue, re2c is registered last.
if (NOT MSVC)
    link_libraries (stdc++)
endif ()

add_subdirectory (re2c)

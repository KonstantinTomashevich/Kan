if (NOT KAN_USE_VULKAN_API)
    return ()
endif ()

register_concrete (context_render_backend_system_vulkan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c" "*.cpp")

concrete_require (
        SCOPE PRIVATE
        ABSTRACT error log memory threading
        THIRD_PARTY volk Vulkan::Headers VulkanMemoryAllocator)

# stdc++ is required by VulkanMemoryAllocator and is not provided by default on non-MSVC compilers.
if (NOT MSVC)
    concrete_require (SCOPE PRIVATE THIRD_PARTY stdc++)
endif ()

concrete_implements_abstract (context_render_backend_system)
register_context_system (render_backend_system_t)
c_interface_scanner_setup (GLOB "*.c" "*.h")

option (KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED "Whether Vulkan validation routine is enabled." ON)
if (KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
    concrete_compile_definitions (PRIVATE KAN_CONTEXT_RENDER_BACKEND_VULKAN_VALIDATION_ENABLED)
endif ()

option (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY
        "Whether memory profiling is enabled for Vulkan allocations." ON)

if (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    concrete_compile_definitions (PRIVATE KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
endif ()

set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT "3" CACHE STRING
        "Maximum amount of frames that can be submitted ahead of time to reduce time spent waiting for GPU if any.")
set (KAN_CONTEXT_RENDER_BACKEND_FENCE_WAIT_TIMEOUT_NS "10000" CACHE STRING
        "Maximum amount amount of milliseconds to wait for rendering fence before skipping frame.")
set (KAN_CONTEXT_RENDER_BACKEND_IMAGE_WAIT_TIMEOUT_NS "10000" CACHE STRING
        "Maximum amount amount of milliseconds to wait for swap chain image before skipping frame.")
set (KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES "16" CACHE STRING
        "Maximum count of handles in any list for Vulkan API call that can be inlined into static array.")

concrete_compile_definitions (
        PRIVATE
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT}
        KAN_CONTEXT_RENDER_BACKEND_FENCE_WAIT_TIMEOUT_NS=${KAN_CONTEXT_RENDER_BACKEND_FENCE_WAIT_TIMEOUT_NS}
        KAN_CONTEXT_RENDER_BACKEND_IMAGE_WAIT_TIMEOUT_NS=${KAN_CONTEXT_RENDER_BACKEND_IMAGE_WAIT_TIMEOUT_NS}
        KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES=${KAN_CONTEXT_RENDER_BACKEND_MAX_INLINE_HANDLES})

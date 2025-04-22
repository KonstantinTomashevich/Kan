if (NOT Vulkan_FOUND)
    return ()
endif ()

register_concrete (context_render_backend_system_vulkan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c" "*.cpp")

concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_profiler error log memory precise_time threading
        THIRD_PARTY volk Vulkan::Headers VulkanMemoryAllocator)

# stdc++ is required by VulkanMemoryAllocator and is not provided by default on non-MSVC compilers.
if (NOT MSVC)
    concrete_require (SCOPE PRIVATE THIRD_PARTY stdc++)
endif ()

concrete_implements_abstract (context_render_backend_system)
register_context_system (NAME render_backend_system_t)

option (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED "Whether Vulkan debug utilities are enabled." ON)
if (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
    concrete_compile_definitions (PRIVATE KAN_CONTEXT_RENDER_BACKEND_VULKAN_DEBUG_ENABLED)
endif ()

option (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PRINT_FRAME_TIMES "Enable GPU frame time printing to log." OFF)
if (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PRINT_FRAME_TIMES)
    concrete_compile_definitions (PRIVATE KAN_CONTEXT_RENDER_BACKEND_VULKAN_PRINT_FRAME_TIMES)
endif ()

option (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY
        "Whether memory profiling is enabled for Vulkan allocations." ON)

if (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
    concrete_compile_definitions (PRIVATE KAN_CONTEXT_RENDER_BACKEND_VULKAN_PROFILE_MEMORY)
endif ()

set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT "3" CACHE STRING
        "Maximum amount of frames that can be submitted ahead of time to reduce time spent waiting for GPU if any.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_FENCE_WAIT_NS "10000" CACHE STRING
        "Maximum amount amount of milliseconds to wait for rendering fence before skipping frame.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_IMAGE_WAIT_NS "10000" CACHE STRING
        "Maximum amount amount of milliseconds to wait for swap chain image before skipping frame.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES "16" CACHE STRING
        "Maximum count of handles in any list for Vulkan API call that can be inlined into static array.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS "8" CACHE STRING
        "Maximum count of memory barriers in any list for Vulkan API call that can be inlined into static array.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS "16" CACHE STRING
        "Maximum count of descriptors in any list for Vulkan API call that can be inlined into static array.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_SCHEDULE_STACK_SIZE "131072" CACHE STRING
        "Size of one stack for stack groups that are used to allocate commands that are being scheduled.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PASS_STACK_SIZE "65536" CACHE STRING
        "Size of one stack for stack group allocator that is used for pass instance data.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_STAGING_PAGE_SIZE "67108864" CACHE STRING
        "Size of a page for frame lifetime allocator that is used to allocate staging space.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS "67" CACHE STRING
        "Initial count of buckets for pipeline parameter set layouts hash storage.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_PL_BUCKETS "67" CACHE STRING
        "Initial count of buckets for shared pipeline layouts hash storage.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_COMPILATION_WAIT_NS "500000" CACHE STRING
        "Time in nanoseconds that is passed to sleep when we're forced to wait for compilation in active thread.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSP_SETS "1024" CACHE STRING
        "Count of maximum sets per descriptor set pool.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_UNIFORM "512" CACHE STRING
        "Default count of uniform buffer bindings for initial descriptor set pool.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_STORAGE "256" CACHE STRING
        "Default count of storage buffer bindings for initial descriptor set pool.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_SAMPLER "128" CACHE STRING
        "Default count of sampler bindings for initial descriptor set pool.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_IMAGE "512" CACHE STRING
        "Default count of image bindings for initial descriptor set pool.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_GCB_ARRAY_SIZE "8" CACHE STRING
        "Initial size of an array that contains secondary graphics command buffers.")
set (KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME "256" CACHE STRING
        "Max length of generated debug name for Vulkan objects.")

concrete_compile_definitions (
        PRIVATE
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_FRAMES_IN_FLIGHT}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_FENCE_WAIT_NS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_FENCE_WAIT_NS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_IMAGE_WAIT_NS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_IMAGE_WAIT_NS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_HANDLES}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_BARRIERS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_INLINE_DESCS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_SCHEDULE_STACK_SIZE=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_SCHEDULE_STACK_SIZE}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_PASS_STACK_SIZE=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_PASS_STACK_SIZE}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_STAGING_PAGE_SIZE=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_STAGING_PAGE_SIZE}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_SET_LAYOUT_BUCKETS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_PL_BUCKETS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_PL_BUCKETS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_COMPILATION_WAIT_NS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_COMPILATION_WAIT_NS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSP_SETS=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSP_SETS}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_UNIFORM=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_UNIFORM}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_STORAGE=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_STORAGE}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_SAMPLER=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_SAMPLER}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_IMAGE=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_DSPD_IMAGE}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_GCB_ARRAY_SIZE=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_GCB_ARRAY_SIZE}
        KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME=${KAN_CONTEXT_RENDER_BACKEND_VULKAN_MAX_DEBUG_NAME})

if (NOT KAN_USE_VULKAN_API)
    return ()
endif ()

register_concrete (render_backend_vulkan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c" "*.cpp")

concrete_require (
        SCOPE PRIVATE
        ABSTRACT error log memory threading
        THIRD_PARTY volk Vulkan::Headers VulkanMemoryAllocator)

concrete_implements_abstract (render_backend)
c_interface_scanner_setup (GLOB "*.c" "*.h")

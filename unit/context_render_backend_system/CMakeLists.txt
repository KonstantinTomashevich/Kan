register_abstract (context_render_backend_system)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE container context context_application_system INTERFACE api_common platform)
c_interface_scanner_setup (GLOB "*.h")

if (KAN_USE_VULKAN_API)
    abstract_register_implementation (NAME vulkan PARTS context_render_backend_system_vulkan)
endif ()

register_concrete (render_backend_tools_vulkan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PRIVATE ABSTRACT error log memory)
concrete_implements_abstract (render_backend_tools)
c_interface_scanner_setup (GLOB "*.c" "*.h")

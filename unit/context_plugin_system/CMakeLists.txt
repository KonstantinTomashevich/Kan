register_concrete (context_plugin_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT error file_system log memory platform
        CONCRETE_INTERFACE context_reflection_system)

register_context_system (plugin_system_t)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE "8" CACHE STRING "Initial size for configuration plugins array.")

concrete_compile_definitions (
        PRIVATE
        KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE=${KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE})

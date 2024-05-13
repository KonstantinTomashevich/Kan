register_concrete (context_plugin_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT error file_system file_system_watcher log memory platform
        CONCRETE_INTERFACE context_reflection_system context_update_system)

register_context_system (plugin_system_t)
c_interface_scanner_setup (GLOB "*.c" "*.h")

set (KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE "8" CACHE STRING "Initial size for configuration plugins array.")
set (KAN_PLUGIN_SYSTEM_HOT_RELOAD_SUFFIX_BUFFER "16" CACHE STRING "Size of a buffer for hot reload suffix generation.")
set (KAN_PLUGIN_SYSTEM_HOT_RELOAD_IO_BUFFER "4096" CACHE STRING "Size of a buffer for hot reload io operations.")

concrete_compile_definitions (
        PRIVATE
        KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE=${KAN_PLUGIN_SYSTEM_PLUGINS_INITIAL_SIZE}
        KAN_PLUGIN_SYSTEM_HOT_RELOAD_SUFFIX_BUFFER=${KAN_PLUGIN_SYSTEM_HOT_RELOAD_SUFFIX_BUFFER}
        KAN_PLUGIN_SYSTEM_HOT_RELOAD_IO_BUFFER=${KAN_PLUGIN_SYSTEM_HOT_RELOAD_IO_BUFFER})

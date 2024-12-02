register_concrete (context_hot_reload_coordination_system_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PRIVATE
        ABSTRACT error file_system_watcher log memory threading
        CONCRETE_INTERFACE context_application_system context_update_system)

concrete_implements_abstract (context_hot_reload_coordination_system)
register_context_system (NAME hot_reload_coordination_system_t)

register_concrete (context_hot_reload_coordination_system_none)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PRIVATE ABSTRACT error log memory threading)

concrete_implements_abstract (context_hot_reload_coordination_system)
register_context_system (NAME hot_reload_coordination_system_t)

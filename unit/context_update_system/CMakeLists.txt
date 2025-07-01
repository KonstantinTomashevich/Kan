register_concrete (context_update_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context)
concrete_require (SCOPE PRIVATE ABSTRACT cpu_profiler error log memory reflection)

register_context_system (NAME update_system_t)
setup_reflected_preprocessing (REFLECTION_GLOB "*.h")

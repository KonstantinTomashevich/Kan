register_concrete (context_update_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context)
concrete_require (SCOPE PRIVATE ABSTRACT cpu_profiler error log memory reflection)

register_context_system (NAME update_system_t)
c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

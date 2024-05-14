register_concrete (inline_math)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_require (SCOPE PUBLIC INTERFACE api_common THIRD_PARTY cglm_headers)
concrete_require (SCOPE PRIVATE ABSTRACT reflection CONCRETE_INTERFACE container)

c_interface_scanner_setup (GLOB "*.c" "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

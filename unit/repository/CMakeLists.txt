register_abstract (repository)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (CONCRETE_INTERFACE container INTERFACE api_common reflection threading)
abstract_register_implementation (NAME kan PARTS reflection_kan)

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)

register_concrete (repository_reflection)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection)
register_abstract (universe_resource_reference)
abstract_include ("${CMAKE_CURRENT_SOURCE_DIR}")
abstract_require (ABSTRACT universe_resource_provider CONCRETE_INTERFACE universe)
abstract_register_implementation (NAME kan PARTS universe_resource_reference_kan universe_resource_reference_reflection)

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)

register_concrete (universe_resource_reference_reflection)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
concrete_require (SCOPE PUBLIC ABSTRACT reflection universe_resource_reference)
register_unit_reflection_with_name (universe_resource_reference)

register_concrete (application_framework_example_import_icon_import)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT image log reflection
        CONCRETE_INTERFACE application_framework_example_import_icon container resource_pipeline)

c_interface_scanner_setup (GLOB "*.c")
reflection_generator_setup (GLOB "*.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

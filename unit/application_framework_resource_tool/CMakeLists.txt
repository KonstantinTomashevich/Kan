register_concrete (application_framework_resource_tool)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT memory reflection CONCRETE_INTERFACE container context)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT file_system log
        CONCRETE_INTERFACE context_plugin_system context_reflection_system context_resource_pipeline_system
        serialization)

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_RESOURCE_PROJECT_IO_BUFFER "4096" CACHE STRING "Size of a buffer for buffered reading of resource projects.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PROJECT_IO_BUFFER=${KAN_RESOURCE_PROJECT_IO_BUFFER})

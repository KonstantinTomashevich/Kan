register_concrete (universe_resource_reference_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PRIVATE
        ABSTRACT log platform universe_resource_reference universe_resource_provider virtual_file_system
        CONCRETE_INTERFACE
        context_plugin_system context_reflection_system context_virtual_file_system serialization resource_pipeline
        universe_resource_reference_reflection)

c_interface_scanner_setup (GLOB "*.c")
reflection_generator_setup (GLOB "*.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")
concrete_implements_abstract (universe_resource_reference)

set (KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK "2048" CACHE STRING
        "Size of a chunk for temporary group stack for worker-related allocations.")
set (KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER "4096" CACHE STRING
        "Size of io buffers used to read and write cache files.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK=${KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK}
        KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER=${KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER})

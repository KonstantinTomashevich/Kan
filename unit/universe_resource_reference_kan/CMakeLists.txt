register_concrete (universe_resource_reference_kan)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")
concrete_implements_abstract (universe_resource_reference)

concrete_require (
        SCOPE PRIVATE
        ABSTRACT log platform precise_time universe_resource_reference universe_resource_provider virtual_file_system
        CONCRETE_INTERFACE
        context_plugin_system context_reflection_system context_resource_pipeline_system context_virtual_file_system
        serialization resource_pipeline universe_resource_reference_reflection)
universe_concrete_preprocessing_queue ()

set (KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK "2048" CACHE STRING
        "Size of a chunk for temporary group stack for worker-related allocations.")
set (KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER "4096" CACHE STRING
        "Size of io buffers used to read and write cache files.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK=${KAN_UNIVERSE_RESOURCE_REFERENCE_TEMPORARY_STACK}
        KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER=${KAN_UNIVERSE_RESOURCE_REFERENCE_IO_BUFFER})

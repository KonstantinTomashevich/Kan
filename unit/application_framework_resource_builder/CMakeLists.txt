register_concrete (application_framework_resource_builder)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        ABSTRACT cpu_dispatch error file_system log memory platform reflection virtual_file_system
        CONCRETE_INTERFACE
        application_framework_resource_tool context context_plugin_system context_reflection_system reflection_helpers
        resource_index resource_pipeline serialization)

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_RESOURCE_BUILDER_IO_BUFFER "4096" CACHE STRING "Size of a buffer for buffered reading and writing.")
set (KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS "128" CACHE STRING
        "Initial count of buckets inside node storages of targets.")
set (KAN_RESOURCE_BUILDER_TEMPORARY_STACK "32768" CACHE STRING "Size of one stack item for temporary allocator.")
set (KAN_RESOURCE_BUILDER_BYPRODUCT_PRODUCED_FROM_SIZE "4" CACHE STRING
        "Initial size for byproduct array that stores info about from which resources byproduct is produced.")
set (KAN_RESOURCE_BUILDER_BYPRODUCT_NAME_BUFFER "255" CACHE STRING "Size of a buffer for generating byproduct name.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_BUILDER_IO_BUFFER=${KAN_RESOURCE_BUILDER_IO_BUFFER}
        KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS=${KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS}
        KAN_RESOURCE_BUILDER_TEMPORARY_STACK=${KAN_RESOURCE_BUILDER_TEMPORARY_STACK}
        KAN_RESOURCE_BUILDER_BYPRODUCT_PRODUCED_FROM_SIZE=${KAN_RESOURCE_BUILDER_BYPRODUCT_PRODUCED_FROM_SIZE}
        KAN_RESOURCE_BUILDER_BYPRODUCT_NAME_BUFFER=${KAN_RESOURCE_BUILDER_BYPRODUCT_NAME_BUFFER})

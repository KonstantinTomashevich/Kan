register_concrete (application_framework_resource_builder)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        ABSTRACT cpu_dispatch error file_system log memory platform reflection virtual_file_system
        CONCRETE_INTERFACE
        application_framework_resource_tool context context_plugin_system context_reflection_system resource_index
        resource_pipeline serialization)

set (KAN_RESOURCE_BUILDER_IO_BUFFER "4096" CACHE STRING "Size of a buffer for buffered reading and writing.")
set (KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS "128" CACHE STRING
        "Initial count of buckets inside node storages of targets.")
set (KAN_RESOURCE_BUILDER_TEMPORARY_STACK "32768" CACHE STRING "Size of one stack item for temporary allocator.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_BUILDER_IO_BUFFER=${KAN_RESOURCE_BUILDER_IO_BUFFER}
        KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS=${KAN_RESOURCE_BUILDER_TARGET_NODES_BUCKETS}
        KAN_RESOURCE_BUILDER_TEMPORARY_STACK=${KAN_RESOURCE_BUILDER_TEMPORARY_STACK})

register_concrete (application_framework_resource_importer)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT checksum cpu_dispatch error file_system log memory platform reflection
        CONCRETE_INTERFACE
        application_framework_resource_tool context context_plugin_system context_reflection_system
        reflection_helpers resource_pipeline serialization)
setup_core_preprocessing ()

set (KAN_RESOURCE_IMPORTER_IO_BUFFER "4096" CACHE STRING "Size of a buffer for buffered reading and writing.")
set (KAN_RESOURCE_IMPORTER_TEMPORARY_STACK "32768 " CACHE STRING "Size of one stack item for temporary allocator.")
set (KAN_RESOURCE_IMPORTER_INPUT_CAPACITY "8" CACHE STRING "Initial capacity for per rule import arrays.")
set (KAN_RESOURCE_IMPORTER_OUTPUT_CAPACITY "8" CACHE STRING "Initial capacity for per input output arrays.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_IMPORTER_IO_BUFFER=${KAN_RESOURCE_IMPORTER_IO_BUFFER}
        KAN_RESOURCE_IMPORTER_TEMPORARY_STACK=${KAN_RESOURCE_IMPORTER_TEMPORARY_STACK}
        KAN_RESOURCE_IMPORTER_INPUT_CAPACITY=${KAN_RESOURCE_IMPORTER_INPUT_CAPACITY}
        KAN_RESOURCE_IMPORTER_OUTPUT_CAPACITY=${KAN_RESOURCE_IMPORTER_OUTPUT_CAPACITY})

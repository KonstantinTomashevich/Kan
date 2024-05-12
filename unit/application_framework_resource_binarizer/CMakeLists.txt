register_concrete (application_framework_resource_binarizer)
concrete_sources ("*.c")
concrete_require (
        SCOPE PRIVATE
        ABSTRACT error file_system log memory reflection
        CONCRETE_INTERFACE
        application_framework_tool context context_reflection_system serialization)

set (KAN_RESOURCE_BINARIZER_IO_BUFFER "4096" CACHE STRING "Size of a buffer for buffered reading and writing.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_BINARIZER_IO_BUFFER=${KAN_RESOURCE_BINARIZER_IO_BUFFER})

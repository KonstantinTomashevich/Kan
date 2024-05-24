register_concrete (application_framework_packer)
concrete_sources ("*.c")
concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_dispatch error file_system log memory reflection virtual_file_system
        CONCRETE_INTERFACE
        application_framework_tool context context_reflection_system resource_index serialization)

set (KAN_PACKER_RESOURCE_STORAGE_BUCKETS "1024" CACHE STRING
        "Initial count of buckets for application framework packer resource storage.")
set (KAN_PACKER_IO_BUFFER_SIZE "4096" CACHE STRING
        "Size of a buffer for application framework packer IO operations.")
set (KAN_PACKER_TEMPORARY_ALLOCATOR_STACK_SIZE "4096" CACHE STRING
        "Size of one stack in temporary allocator for tasks to dispatch allocations.")
set (KAN_PACKER_PACK_PATH_MAX "512" CACHE STRING "Maximum size of path inside generated pack.")

concrete_compile_definitions (
        PRIVATE
        KAN_PACKER_RESOURCE_STORAGE_BUCKETS=${KAN_PACKER_RESOURCE_STORAGE_BUCKETS}
        KAN_PACKER_IO_BUFFER_SIZE=${KAN_PACKER_IO_BUFFER_SIZE}
        KAN_PACKER_TEMPORARY_ALLOCATOR_STACK_SIZE=${KAN_PACKER_TEMPORARY_ALLOCATOR_STACK_SIZE}
        KAN_PACKER_PACK_PATH_MAX=${KAN_PACKER_PACK_PATH_MAX})

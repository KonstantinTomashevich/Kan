register_concrete (resource_pipeline_build)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_pipeline_tooling)
concrete_require (
        SCOPE PRIVATE 
        ABSTRACT cpu_dispatch error file_system log memory platform precise_time virtual_file_system
        CONCRETE_INTERFACE reflection_helpers serialization)

# There should be nothing valuable for the reflection in this unit.
# And keeping it out of the reflection makes it possible to simplify internal type names.
setup_core_preprocessing ()

set (KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS "257" CACHE STRING
        "Initial count of buckets for resource entries in resource type storage.")
set (KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS "67" CACHE STRING
        "Initial count of buckets for resource type storages in resource build target.")
set (KAN_RESOURCE_PIPELINE_BUILD_THIRD_PARTY_BUCKETS "257" CACHE STRING
        "Initial count of buckets for raw third party resources in resource build target.")
set (KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER "16384" CACHE STRING 
        "Default stream read and write buffer size for IO operations.")
set (KAN_RESOURCE_PIPELINE_BUILD_TPC_ENTRIES_PER_LAYER "32" CACHE STRING
        "Initial capacity of entries per layer for transient platform configuration data.")
set (KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS "32" CACHE STRING
        "Initial bucket count of instantiated platform configuration hash storage.")
set (KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER "16384" CACHE STRING
        "Size of a buffer for formatting circular reference inside resource chain request error message.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_THIRD_PARTY_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_THIRD_PARTY_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER=${KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER}
        KAN_RESOURCE_PIPELINE_BUILD_TPC_ENTRIES_PER_LAYER=${KAN_RESOURCE_PIPELINE_BUILD_TPC_ENTRIES_PER_LAYER}
        KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER=${KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER})

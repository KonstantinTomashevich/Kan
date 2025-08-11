register_concrete (resource_pipeline_build)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT log CONCRETE_INTERFACE resource_pipeline_tooling)
concrete_require (
        SCOPE PRIVATE 
        ABSTRACT cpu_dispatch error file_system memory platform precise_time virtual_file_system
        CONCRETE_INTERFACE reflection_helpers serialization
        THIRD_PARTY qsort)

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
set (KAN_RESOURCE_PIPELINE_BUILD_RRDM_INITIAL "32" CACHE STRING
        "Initial size of an array where root resources for deployment mark are stored in the beginning of the build.")
set (KAN_RESOURCE_PIPELINE_BUILD_WORKING_CHECK_DELAY_NS "1000000" CACHE STRING
        "Delay between checks on working thread whether task threads are still busy building resources.")
set (KAN_RESOURCE_PIPELINE_BUILD_LOG_ENTRIES_CAPACITY "512" CACHE STRING
        "Base capacity for resource entry arrays in every target when log generation begins.")
set (KAN_RESOURCE_PIPELINE_BUILD_PACK_ENTRIES_CAPACITY "1024" CACHE STRING
        "Base capacity for resource entry array to be packed for target.")
set (KAN_RESOURCE_PIPELINE_BUILD_PACK_INDEX_ITEM_CAPACITY "256" CACHE STRING
        "Base capacity for array of items of the same type in packed resource index.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_RESOURCE_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_TYPES_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_THIRD_PARTY_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_THIRD_PARTY_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER=${KAN_RESOURCE_PIPELINE_BUILD_IO_BUFFER}
        KAN_RESOURCE_PIPELINE_BUILD_TPC_ENTRIES_PER_LAYER=${KAN_RESOURCE_PIPELINE_BUILD_TPC_ENTRIES_PER_LAYER}
        KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS=${KAN_RESOURCE_PIPELINE_BUILD_PC_BUCKETS}
        KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER=${KAN_RESOURCE_PIPELINE_BUILD_CR_MESSAGE_BUFFER}
        KAN_RESOURCE_PIPELINE_BUILD_RRDM_INITIAL=${KAN_RESOURCE_PIPELINE_BUILD_RRDM_INITIAL}
        KAN_RESOURCE_PIPELINE_BUILD_WORKING_CHECK_DELAY_NS=${KAN_RESOURCE_PIPELINE_BUILD_WORKING_CHECK_DELAY_NS}
        KAN_RESOURCE_PIPELINE_BUILD_LOG_ENTRIES_CAPACITY=${KAN_RESOURCE_PIPELINE_BUILD_LOG_ENTRIES_CAPACITY}
        KAN_RESOURCE_PIPELINE_BUILD_PACK_ENTRIES_CAPACITY=${KAN_RESOURCE_PIPELINE_BUILD_PACK_ENTRIES_CAPACITY}
        KAN_RESOURCE_PIPELINE_BUILD_PACK_INDEX_ITEM_CAPACITY=${KAN_RESOURCE_PIPELINE_BUILD_PACK_INDEX_ITEM_CAPACITY})

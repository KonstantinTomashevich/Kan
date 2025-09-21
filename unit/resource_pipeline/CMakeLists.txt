register_concrete (resource_pipeline)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection stream CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log reflection_helpers)
setup_reflected_preprocessing ()

set (KAN_RESOURCE_PIPELINE_RD_RESOURCE_TYPE_BUCKETS "67" CACHE STRING
        "Initial count of buckets for scanned information about resource types in resources reflected data.")
set (KAN_RESOURCE_PIPELINE_RD_REFERENCER_STRUCTS_BUCKETS "129" CACHE STRING
        "Initial count of buckets for scanned information about referencer structs in resources reflected data.")
set (KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK "8" CACHE STRING
        "Max size of patch section stack for reference detection algorithm.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_RD_RESOURCE_TYPE_BUCKETS=${KAN_RESOURCE_PIPELINE_RD_RESOURCE_TYPE_BUCKETS}
        KAN_RESOURCE_PIPELINE_RD_REFERENCER_STRUCTS_BUCKETS=${KAN_RESOURCE_PIPELINE_RD_REFERENCER_STRUCTS_BUCKETS}
        KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK=${KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK})

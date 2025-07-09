register_concrete (resource_pipeline_tooling)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection stream CONCRETE_INTERFACE container resource_pipeline_runtime)
concrete_require (SCOPE PRIVATE ABSTRACT error log reflection_helpers)
setup_reflected_preprocessing ()

set (KAN_RESOURCE_PIPELINE_TOOLING_RDRT_BUCKETS "67" CACHE STRING
        "Initial count of buckets for scanned information about resource types in resources reflected data.")
set (KAN_RESOURCE_PIPELINE_TOOLING_RDRS_BUCKETS "129" CACHE STRING
        "Initial count of buckets for scanned information about referencer structs in resources reflected data.")
set (KAN_RESOURCE_PIPELINE_TOOLING_PATCH_SECTION_STACK "8" CACHE STRING 
        "Max size of patch section stack for reference detection algorithm.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_TOOLING_RDRT_BUCKETS=${KAN_RESOURCE_PIPELINE_TOOLING_RDRT_BUCKETS}
        KAN_RESOURCE_PIPELINE_TOOLING_RDRS_BUCKETS=${KAN_RESOURCE_PIPELINE_TOOLING_RDRS_BUCKETS}
        KAN_RESOURCE_PIPELINE_TOOLING_PATCH_SECTION_STACK=${KAN_RESOURCE_PIPELINE_TOOLING_PATCH_SECTION_STACK})

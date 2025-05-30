register_concrete (resource_pipeline)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection stream CONCRETE_INTERFACE container reflection_helpers)
concrete_require (SCOPE PRIVATE ABSTRACT error log)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

set (KAN_RESOURCE_PIPELINE_SCAN_BUCKETS "67" CACHE STRING
        "Initial count of buckets for scanned information about resource types and references in them.")
set (KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE "64" CACHE STRING
        "Initial size for info arrays for scanned types in reference scan.")
set (KAN_RESOURCE_PIPELINE_DETECTED_ARRAY_INITIAL_SIZE "8" CACHE STRING "Initial size for detected reference arrays.")
set (KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK_SIZE "8" CACHE STRING "Max size of patch section stack.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_SCAN_BUCKETS=${KAN_RESOURCE_PIPELINE_SCAN_BUCKETS}
        KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE=${KAN_RESOURCE_PIPELINE_SCAN_ARRAY_INITIAL_SIZE}
        KAN_RESOURCE_PIPELINE_DETECTED_ARRAY_INITIAL_SIZE=${KAN_RESOURCE_PIPELINE_DETECTED_ARRAY_INITIAL_SIZE}
        KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK_SIZE=${KAN_RESOURCE_PIPELINE_PATCH_SECTION_STACK_SIZE})

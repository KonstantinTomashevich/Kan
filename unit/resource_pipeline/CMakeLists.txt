register_concrete (resource_pipeline)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection stream CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log)

c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_RESOURCE_PIPELINE_SCAN_BUCKETS "64" CACHE STRING
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

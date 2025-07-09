register_concrete (resource_pipeline_runtime)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection stream CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log)
setup_reflected_preprocessing ()

set (KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_ITEMS "64" CACHE STRING "Initial size of resource index item container.")
set (KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_TYPES "16" CACHE STRING "Initial size of resource index types container.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_ITEMS=${KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_ITEMS}
        KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_TYPES=${KAN_RESOURCE_PIPELINE_RUNTIME_INDEX_BASE_TYPES})

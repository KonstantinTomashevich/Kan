register_concrete (resource_index)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT log)

concrete_preprocessing_queue_step_cushion ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

set (KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL "16" CACHE STRING
        "Initial size of item array inside native container for serialized resource index.")
set (KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL "16" CACHE STRING
        "Initial size of native container array for serialized resource index.")
set (KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL "16" CACHE STRING
        "Initial size of third party item array for serialized resource index.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL=${KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL}
        KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL=${KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL}
        KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL=${KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL})

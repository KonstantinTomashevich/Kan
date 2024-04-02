register_concrete (resource_index)
concrete_include (PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT log)

c_interface_scanner_setup (GLOB "*.h" "*.c")
reflection_generator_setup (GLOB "*.h" "*.c")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL "16" CACHE STRING
        "Initial size of item array inside native container for serialized resource index.")
set (KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL "16" CACHE STRING
        "Initial size of native container array for serialized resource index.")
set (KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL "16" CACHE STRING
        "Initial size of third party item array for serialized resource index.")

set (KAN_RESOURCE_INDEX_NATIVE_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for native resources inside resource index.")
set (KAN_RESOURCE_INDEX_NATIVE_LOAD_FACTOR "4" CACHE STRING
        "Target count of native resources inside bucket for resource index.")

set (KAN_RESOURCE_INDEX_THIRD_PARTY_INITIAL_BUCKETS "64" CACHE STRING
        "Initial count of buckets for third party resources inside resource index.")
set (KAN_RESOURCE_INDEX_THIRD_PARTY_LOAD_FACTOR "4" CACHE STRING
        "Target count of third party resources inside bucket for resource index.")

concrete_compile_definitions (
        PRIVATE
        KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL=${KAN_RESOURCE_INDEX_NATIVE_CONTAINER_INITIAL}
        KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL=${KAN_RESOURCE_INDEX_NATIVE_ARRAY_INITIAL}
        KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL=${KAN_RESOURCE_INDEX_THIRD_PARTY_ARRAY_INITIAL}
        KAN_RESOURCE_INDEX_NATIVE_INITIAL_BUCKETS=${KAN_RESOURCE_INDEX_NATIVE_INITIAL_BUCKETS}
        KAN_RESOURCE_INDEX_NATIVE_LOAD_FACTOR=${KAN_RESOURCE_INDEX_NATIVE_LOAD_FACTOR}
        KAN_RESOURCE_INDEX_THIRD_PARTY_INITIAL_BUCKETS=${KAN_RESOURCE_INDEX_THIRD_PARTY_INITIAL_BUCKETS}
        KAN_RESOURCE_INDEX_THIRD_PARTY_LOAD_FACTOR=${KAN_RESOURCE_INDEX_THIRD_PARTY_LOAD_FACTOR})

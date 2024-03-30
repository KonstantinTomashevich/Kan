register_concrete (universe)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT repository workflow CONCRETE_INTERFACE container context)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT error log memory
        CONCRETE_INTERFACE context_reflection_system context_update_system)
register_context_system (universe_system_t)

c_interface_scanner_setup (GLOB "*.c" "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS "16" CACHE STRING
        "Initial buckets count for scanned scheduler apis storage.")
set (KAN_UNIVERSE_SCHEDULER_LOAD_FACTOR "4" CACHE STRING "Target count of scanned scheduler apis per bucket.")

set (KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS "64" CACHE STRING "Initial buckets count for scanned mutator apis storage.")
set (KAN_UNIVERSE_MUTATOR_LOAD_FACTOR "4" CACHE STRING "Target count of scanned mutator apis per bucket.")

set (KAN_UNIVERSE_GROUP_INITIAL_BUCKETS "32" CACHE STRING "Initial buckets count for scanned mutator group storage.")
set (KAN_UNIVERSE_GROUP_LOAD_FACTOR "16" CACHE STRING "Target count of group multi map items per bucket.")

set (KAN_UNIVERSE_MIGRATION_INITIAL_STACK "131072" CACHE STRING
        "Size of one stack for stack group allocator for temporary allocations inside migration algorithm.")

set (KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS "8" CACHE STRING
        "Max count of parts for names of automated query. Prefix is excluded.")

set (KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH "128" CACHE STRING
        "Max length of record type for automated query.")

set (KAN_UNIVERSE_DEPLOY_INITIAL_STACK "131072" CACHE STRING
        "Size of one stack for stack group allocator for temporary allocations inside world deploy algorithm.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS=${KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS}
        KAN_UNIVERSE_SCHEDULER_LOAD_FACTOR=${KAN_UNIVERSE_SCHEDULER_LOAD_FACTOR}
        KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS=${KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS}
        KAN_UNIVERSE_MUTATOR_LOAD_FACTOR=${KAN_UNIVERSE_MUTATOR_LOAD_FACTOR}
        KAN_UNIVERSE_MIGRATION_INITIAL_STACK=${KAN_UNIVERSE_MIGRATION_INITIAL_STACK}
        KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS=${KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS}
        KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH=${KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH}
        KAN_UNIVERSE_DEPLOY_INITIAL_STACK=${KAN_UNIVERSE_DEPLOY_INITIAL_STACK}
        KAN_UNIVERSE_GROUP_INITIAL_BUCKETS=${KAN_UNIVERSE_GROUP_INITIAL_BUCKETS}
        KAN_UNIVERSE_GROUP_LOAD_FACTOR=${KAN_UNIVERSE_GROUP_LOAD_FACTOR})

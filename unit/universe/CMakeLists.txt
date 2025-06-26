register_concrete (universe)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (
        SCOPE PUBLIC
        # error is in public requirements as some of the convenience macros use it for proper validation.
        ABSTRACT error repository workflow 
        CONCRETE_INTERFACE container context THIRD_PARTY qsort)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_profiler log memory
        CONCRETE_INTERFACE context_reflection_system context_update_system)
register_context_system (NAME universe_system_t)

concrete_preprocessing_queue_step_cushion ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

set (KAN_UNIVERSE_SCHEDULER_INITIAL_BUCKETS "17" CACHE STRING
        "Initial buckets count for scanned scheduler apis storage.")
set (KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS "67" CACHE STRING "Initial buckets count for scanned mutator apis storage.")
set (KAN_UNIVERSE_GROUP_INITIAL_BUCKETS "37" CACHE STRING "Initial buckets count for scanned mutator group storage.")
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
        KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS=${KAN_UNIVERSE_MUTATOR_INITIAL_BUCKETS}
        KAN_UNIVERSE_MIGRATION_INITIAL_STACK=${KAN_UNIVERSE_MIGRATION_INITIAL_STACK}
        KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS=${KAN_UNIVERSE_MAX_AUTOMATED_QUERY_NAME_PARTS}
        KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH=${KAN_UNIVERSE_MAX_AUTOMATED_QUERY_TYPE_LENGTH}
        KAN_UNIVERSE_DEPLOY_INITIAL_STACK=${KAN_UNIVERSE_DEPLOY_INITIAL_STACK}
        KAN_UNIVERSE_GROUP_INITIAL_BUCKETS=${KAN_UNIVERSE_GROUP_INITIAL_BUCKETS})

register_concrete (context_universe_world_definition_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context universe)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT context_hot_reload_coordination_system cpu_profiler error log memory precise_time
        CONCRETE_INTERFACE context_reflection_system context_update_system context_virtual_file_system serialization)

register_context_system (NAME universe_world_definition_system_t)
concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

set (KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS "17" CACHE STRING
        "Initial count of buckets for universe world definition system storage.")

set (KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER "4096" CACHE STRING
        "Size of a buffer to universe world definition system io operations.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS=${KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS}
        KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER=${KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER})

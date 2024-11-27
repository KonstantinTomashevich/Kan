register_concrete (context_universe_world_definition_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE context universe)
concrete_require (
        SCOPE PRIVATE
        ABSTRACT cpu_profiler error log memory platform
        CONCRETE_INTERFACE context_reflection_system context_update_system context_virtual_file_system serialization)

register_context_system (NAME universe_world_definition_system_t)
c_interface_scanner_setup (GLOB "*.h")
reflection_generator_setup (GLOB "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS "16" CACHE STRING
        "Initial count of buckets for universe world definition system storage.")

set (KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER "4096" CACHE STRING
        "Size of a buffer to universe world definition system io operations.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS=${KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_BUCKETS}
        KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER=${KAN_UNIVERSE_WORLD_DEFINITION_SYSTEM_IO_BUFFER})

register_concrete (universe_transform)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE inline_math universe universe_object)
concrete_require (SCOPE PRIVATE ABSTRACT log CONCRETE_INTERFACE universe_time)

c_interface_scanner_setup (GLOB "*.c" "*.h")
reflection_generator_setup (GLOB "*.c" "*.h")
register_unit_reflection ()

reflection_generator_get_output_file_path (REFLECTION_GENERATOR_OUTPUT_FILE_PATH)
concrete_sources_direct ("${REFLECTION_GENERATOR_OUTPUT_FILE_PATH}")

set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_STACK "4096" CACHE STRING
        "Size of one stack item for visual sync task stack group allocator.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_STACK=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_STACK})

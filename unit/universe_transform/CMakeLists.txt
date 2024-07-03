register_concrete (universe_transform)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
universe_concrete_setup_sources (GLOB "*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE inline_math universe universe_object)
concrete_require (SCOPE PRIVATE ABSTRACT log CONCRETE_INTERFACE universe_time)

set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK "4096" CACHE STRING
        "Size of one stack item for visual sync invalidate task stack group allocator.")
set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK "4096" CACHE STRING
        "Size of one stack item for visual sync calculate task stack group allocator.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK}
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK})

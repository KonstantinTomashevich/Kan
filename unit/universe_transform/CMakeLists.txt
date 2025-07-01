register_concrete (universe_transform)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources (GLOB "*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE inline_math universe universe_object)
concrete_require (SCOPE PRIVATE ABSTRACT log CONCRETE_INTERFACE universe_time)
setup_reflected_preprocessing ()

set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK "8196" CACHE STRING
        "Size of one stack item for visual sync invalidate task stack group allocator.")
set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK "8196" CACHE STRING
        "Size of one stack item for visual sync calculate task stack group allocator.")
set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_MIN "8" CACHE STRING
        "Minimum count of transform syncs per batch for task batching.")
set (KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_ATE "256" CACHE STRING
        "Average total expectation for transform syncs with task batching.")

concrete_compile_definitions (
        PRIVATE
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_INV_TASK_STACK}
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_CALC_TASK_STACK}
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_MIN=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_MIN}
        KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_ATE=${KAN_UNIVERSE_TRANSFORM_VISUAL_SYNC_TASK_BATCH_ATE})

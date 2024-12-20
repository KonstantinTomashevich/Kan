register_concrete (context_reflection_system)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE context)
concrete_require (SCOPE PRIVATE ABSTRACT cpu_dispatch error log memory)

register_context_system (NAME reflection_system_t)
concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

set (KAN_REFLECTION_SYSTEM_GENERATION_STACK_SIZE "131072" CACHE STRING
        "Size of a stack chunk for reflection generation algorithm.")

concrete_compile_definitions (
        PRIVATE
        KAN_REFLECTION_SYSTEM_GENERATION_STACK_SIZE=${KAN_REFLECTION_SYSTEM_GENERATION_STACK_SIZE})

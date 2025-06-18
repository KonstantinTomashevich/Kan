register_concrete (inline_math)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC INTERFACE api_common THIRD_PARTY cglm_headers)
concrete_require (SCOPE PRIVATE ABSTRACT reflection CONCRETE_INTERFACE container)

concrete_preprocessing_queue_step_cushion ()
reflection_preprocessor_setup_step (GLOB "*.h")
register_unit_reflection ()

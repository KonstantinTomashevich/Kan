register_concrete (resource_texture_import)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_texture)
concrete_require (SCOPE PRIVATE ABSTRACT error image log CONCRETE_INTERFACE resource_pipeline)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

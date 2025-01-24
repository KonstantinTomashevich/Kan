register_concrete (resource_material)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC
        ABSTRACT context_render_backend_system
        CONCRETE_INTERFACE container render_pipeline_language
        THIRD_PARTY qsort)
concrete_require (SCOPE PRIVATE ABSTRACT error log CONCRETE_INTERFACE resource_pipeline)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

register_concrete (resource_material)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

# Remark: material resource does not use texture directly, but instances reference texture resources.
#         Therefore, it is logical to add dependency on texture resource so it is not missed.
concrete_require (SCOPE PUBLIC
        ABSTRACT context_render_backend_system
        CONCRETE_INTERFACE container inline_math render_pipeline_language resource_texture
        THIRD_PARTY qsort)
concrete_require (SCOPE PRIVATE ABSTRACT error log CONCRETE_INTERFACE resource_pipeline)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

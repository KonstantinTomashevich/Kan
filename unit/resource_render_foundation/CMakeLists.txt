register_concrete (resource_render_foundation)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT context_render_backend_system
        CONCRETE_INTERFACE container render_pipeline_language)
concrete_require (SCOPE PRIVATE CONCRETE_INTERFACE resource_pipeline)
setup_reflected_preprocessing ()

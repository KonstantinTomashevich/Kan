register_concrete (application_framework_examples_icon_import)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (
        SCOPE PUBLIC
        ABSTRACT image log reflection
        CONCRETE_INTERFACE application_framework_examples_icon_resource container resource_pipeline)

concrete_preprocessing_queue_step_cushion ()
reflection_preprocessor_setup_step (GLOB "*.c")
register_unit_reflection ()

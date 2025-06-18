register_concrete (application_framework_examples_icon_resource)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE container resource_pipeline)

concrete_preprocessing_queue_step_cushion ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

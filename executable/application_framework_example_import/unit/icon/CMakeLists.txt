register_concrete (application_framework_example_import_icon)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")
concrete_require (SCOPE PUBLIC ABSTRACT reflection CONCRETE_INTERFACE container resource_pipeline)

concrete_preprocessing_queue_step_preprocess ()
reflection_preprocessor_setup_step (GLOB "*.c" "*.h")
register_unit_reflection ()

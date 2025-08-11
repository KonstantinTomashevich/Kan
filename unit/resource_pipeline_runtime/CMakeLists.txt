register_concrete (resource_pipeline_runtime)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC ABSTRACT reflection stream CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE ABSTRACT error log)
setup_reflected_preprocessing ()

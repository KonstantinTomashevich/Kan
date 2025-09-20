register_concrete (resource_test_expectation)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE container)
concrete_require (SCOPE PRIVATE CONCRETE_INTERFACE resource_pipeline)
setup_reflected_preprocessing ()

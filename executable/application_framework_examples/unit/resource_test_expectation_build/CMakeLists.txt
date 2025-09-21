register_concrete (resource_test_expectation_build)
concrete_include (PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
concrete_sources ("*.c")

concrete_require (SCOPE PUBLIC CONCRETE_INTERFACE resource_test_expectation)
concrete_require (SCOPE PRIVATE ABSTRACT error file_system image log CONCRETE_INTERFACE resource_pipeline)
setup_reflected_preprocessing ()
